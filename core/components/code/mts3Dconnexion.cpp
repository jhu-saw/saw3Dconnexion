/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

/*
  Author(s):  Anton Deguet
  Created on: 2026-05-30

  (C) Copyright 2026 Johns Hopkins University (JHU), All Rights Reserved.

--- begin cisst license - do not edit ---

This software is provided "as is" under an open source license, with
no warranty.  The complete license can be found in license.txt and
http://www.cisst.org/cisst/license.txt.

--- end cisst license ---
*/

#include <hidapi/hidapi.h>

#include <saw3Dconnexion/mts3Dconnexion.h>

#include <cisstCommon/cmnDataFunctionsJSON.h>
#include <cisstMultiTask/mtsManagerLocal.h>
#include <cisstParameterTypes/prmEventButton.h>
#include <cisstParameterTypes/prmPositionCartesianGet.h>
#include <cisstParameterTypes/prmPositionCartesianSet.h>
#include <cisstParameterTypes/prmVelocityCartesianGet.h>
#include <cisstVector/vctTransformationTypes.h>

#include <json/json.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>

CMN_IMPLEMENT_SERVICES_DERIVED_ONEARG(mts3Dconnexion, mtsTaskContinuous, mtsTaskContinuousConstructorArg);

namespace {

    static std::string WideToString(const wchar_t * _value)
    {
        if (!_value) {
            return "";
        }
        const std::wstring wide(_value);
        return std::string(wide.begin(), wide.end());
    }

    static std::wstring StringToWide(const std::string & _value)
    {
        return std::wstring(_value.begin(), _value.end());
    }

    static unsigned short TextToUnsignedShort(const std::string & _text,
                                              const unsigned short _default_value)
    {
        if (_text.empty()) {
            return _default_value;
        }
        const int base = (_text.find("0x") == 0 || _text.find("0X") == 0) ? 0 : 16;
        return static_cast<unsigned short>(std::stoul(_text, nullptr, base));
    }

    static double ApplyDeadbandAndScale(const double _raw,
                                        const double _full_scale,
                                        const double _deadband,
                                        const double _scale)
    {
        if (_full_scale <= 0.0) {
            return 0.0;
        }
        double normalized = _raw / _full_scale;
        normalized = std::max(-1.0, std::min(1.0, normalized));
        const double magnitude = std::abs(normalized);
        if (magnitude <= _deadband) {
            return 0.0;
        }
        const double adjusted_magnitude = (magnitude - _deadband)
            / std::max(1.0e-6, 1.0 - _deadband);
        return std::copysign(adjusted_magnitude * _scale, normalized);
    }

    static int16_t ReadInt16LE(const unsigned char * _data,
                               const size_t _offset)
    {
        const uint16_t value = static_cast<uint16_t>(_data[_offset])
            | (static_cast<uint16_t>(_data[_offset + 1]) << 8);
        return static_cast<int16_t>(value);
    }

    static std::string IdToString(const unsigned short _value)
    {
        std::stringstream stream;
        stream << "0x" << std::hex << std::setw(4) << std::setfill('0') << _value;
        return stream.str();
    }

} // namespace

mts3Dconnexion::mts3Dconnexion(const std::string & _component_name):
    mtsTaskContinuous(_component_name, 256)
{
    Init();
}

mts3Dconnexion::mts3Dconnexion(const mtsTaskContinuousConstructorArg & _arg):
    mtsTaskContinuous(_arg)
{
    Init();
}

mts3Dconnexion::~mts3Dconnexion(void)
{
    Cleanup();
}

void mts3Dconnexion::Init(void)
{
    m_configured = false;
    m_config.name = "three_dconnexion";
    m_vendor_id = 0x256f;
    m_product_id = 0;
    m_match_product_id = false;
    m_handle = nullptr;
    m_interface = nullptr;
    m_last_time = 0.0;
    m_sent_connection_error = false;
    m_button_states.assign(64, false);
    m_raw_linear.Zeros();
    m_raw_angular.Zeros();
}

std::string mts3Dconnexion::reference_frame(void) const
{
    return m_config.name + "_base";
}

const std::string & mts3Dconnexion::moving_frame(void) const
{
    return m_config.name;
}

bool mts3Dconnexion::ApplyConfiguration(void)
{
    if (m_config.name.empty()) {
        CMN_LOG_CLASS_INIT_ERROR << "Configure: \"name\" must not be empty" << std::endl;
        return false;
    }

    try {
        m_vendor_id = TextToUnsignedShort(m_config.vendor_id, m_vendor_id);
        m_match_product_id = !m_config.product_id.empty();
        if (m_match_product_id) {
            m_product_id = TextToUnsignedShort(m_config.product_id, m_product_id);
        }
    } catch (std::exception & _exception) {
        CMN_LOG_CLASS_INIT_ERROR << "Configure: invalid USB vendor_id/product_id ("
                                 << _exception.what() << ")" << std::endl;
        return false;
    }

    m_config.gripper.initial = std::max(m_config.gripper.min,
                                        std::min(m_config.gripper.max,
                                                 m_config.gripper.initial));

    for (auto & button : m_config.buttons) {
        if (button.name.empty()) {
            std::stringstream name;
            name << "button_" << button.index;
            button.name = name.str();
        }
    }

    return true;
}

bool mts3Dconnexion::DeviceMatches(const hid_device_info * _device) const
{
    if (!_device) {
        return false;
    }
    const bool vendor_matches = _device->vendor_id == m_vendor_id;
    const bool product_matches = (!m_match_product_id)
        || (_device->product_id == m_product_id);
    const bool serial_matches = m_config.serial.empty()
        || (WideToString(_device->serial_number) == m_config.serial);
    return vendor_matches && product_matches && serial_matches;
}

std::string mts3Dconnexion::DeviceSearchDescription(void) const
{
    std::stringstream stream;
    stream << "vendor " << IdToString(m_vendor_id);
    if (m_match_product_id) {
        stream << ", product " << IdToString(m_product_id);
    }
    if (!m_config.serial.empty()) {
        stream << ", serial \"" << m_config.serial << "\"";
    }
    if (!m_config.path.empty()) {
        stream << ", path \"" << m_config.path << "\"";
    }
    return stream.str();
}

void mts3Dconnexion::EnumerateDevices(void) const
{
    struct hid_device_info * devs = hid_enumerate(0x0, 0x0);
    struct hid_device_info * current = devs;
    std::set<std::string> reported_devices;
    size_t device_count = 0;
    size_t matching_device_count = 0;
    while (current) {
        std::stringstream device_key;
        device_key << current->vendor_id << ":"
                   << current->product_id << ":"
                   << (current->path ? current->path : "") << ":"
                   << current->interface_number;
        if (!reported_devices.insert(device_key.str()).second) {
            current = current->next;
            continue;
        }

        ++device_count;
        const bool matches = DeviceMatches(current);
        if (matches) {
            ++matching_device_count;
        }
        std::stringstream message;
        message << "HID device found: vendor=" << IdToString(current->vendor_id)
                << ", product=" << IdToString(current->product_id)
                << ", path=" << (current->path ? current->path : "")
                << ", manufacturer=" << WideToString(current->manufacturer_string)
                << ", product-string=" << WideToString(current->product_string)
                << ", serial=" << WideToString(current->serial_number)
                << ", interface=" << current->interface_number;
        if (matches) {
            message << " [matches search]";
        }
        CMN_LOG_CLASS_INIT_WARNING << message.str() << std::endl;
        current = current->next;
    }
    if (device_count == 0) {
        CMN_LOG_CLASS_INIT_WARNING << "No HID devices visible to hidapi" << std::endl;
    } else if (matching_device_count == 0) {
        CMN_LOG_CLASS_INIT_WARNING << "No HID devices matched "
                                   << DeviceSearchDescription() << std::endl;
    }
    hid_free_enumeration(devs);
}

void mts3Dconnexion::OpenDevice(void)
{
    if (m_config.path != "") {
        m_handle = hid_open_path(m_config.path.c_str());
        if (!m_handle) {
            CMN_LOG_CLASS_INIT_WARNING
                << "OpenDevice: failed to open configured HID path \""
                << m_config.path << "\"" << std::endl;
        }
        return;
    }

    struct hid_device_info * devs = hid_enumerate(m_vendor_id,
                                                  m_match_product_id ? m_product_id : 0x0);
    struct hid_device_info * current = devs;
    std::set<std::string> tried_paths;
    while (current) {
        if (DeviceMatches(current) && current->path) {
            if (!tried_paths.insert(current->path).second) {
                current = current->next;
                continue;
            }
            m_handle = hid_open_path(current->path);
            if (m_handle) {
                break;
            }
            CMN_LOG_CLASS_INIT_WARNING
                << "OpenDevice: found matching HID device but failed to open path \""
                << current->path << "\".  Check /dev/hidraw permissions or udev rules."
                << std::endl;
        }
        current = current->next;
    }
    hid_free_enumeration(devs);

    if (!m_handle && m_match_product_id) {
        if (m_config.serial.empty()) {
            m_handle = hid_open(m_vendor_id, m_product_id, nullptr);
        } else {
            const std::wstring serial = StringToWide(m_config.serial);
            m_handle = hid_open(m_vendor_id, m_product_id, serial.c_str());
        }
    }
}

void mts3Dconnexion::ConfigureInterface(void)
{
    m_operating_state.IsBusy() = false;
    m_operating_state.IsHomed() = true;
    m_operating_state.State() = prmOperatingState::ENABLED;
    m_operating_state.Valid() = true;

    m_local_measured_cs.MovingFrame() = moving_frame();
    m_local_measured_cs.ReferenceFrame() = reference_frame();
    m_local_measured_cs.Position().Assign(vctFrm3::Identity());
    m_local_measured_cs.PositionIsValid() = true;
    m_local_measured_cs.Velocity().Assign(vct6(0.0));
    m_local_measured_cs.VelocityIsValid() = true;
    m_local_measured_cs.Force().Assign(vct6(0.0));
    m_local_measured_cs.ForceIsValid() = false;
    update_measured_cs();

    m_gripper_measured_js.SetSize(1);
    m_gripper_measured_js.Name()[0] = "gripper";
    m_gripper_measured_js.Position()[0] = m_config.gripper.initial;
    m_gripper_measured_js.Velocity()[0] = 0.0;
    m_gripper_measured_js.Effort()[0] = 0.0;
    m_gripper_measured_js.SetValid(m_config.gripper.enabled);

    m_gripper_configuration_js.Name().resize(1);
    m_gripper_configuration_js.Name()[0] = "gripper";
    m_gripper_configuration_js.Type().SetSize(1);
    m_gripper_configuration_js.Type()[0] = CMN_JOINT_REVOLUTE;
    m_gripper_configuration_js.PositionMin().SetSize(1);
    m_gripper_configuration_js.PositionMin()[0] = m_config.gripper.min;
    m_gripper_configuration_js.PositionMax().SetSize(1);
    m_gripper_configuration_js.PositionMax()[0] = m_config.gripper.max;

    StateTable.SetAutomaticAdvance(false);
    StateTable.AddData(m_operating_state, "operating_state");
    StateTable.AddData(m_config.base_frame, "base_frame");
    StateTable.AddData(m_local_measured_cs, "local/measured_cs");
    StateTable.AddData(m_measured_cs, "measured_cs");
    if (m_config.gripper.enabled) {
        StateTable.AddData(m_gripper_measured_js, "gripper/measured_js");
    }

    for (const auto & button_configuration : m_config.buttons) {
        m_buttons.push_back(ButtonData());
        ButtonData & button = m_buttons.back();
        button.index = button_configuration.index;
        button.name = button_configuration.name;
        button.suppress_events = button_configuration.suppress_events;
        button.pressed = false;

        const std::string interface_name = m_config.name + "/" + button.name;
        mtsInterfaceProvided * button_interface = AddInterfaceProvided(interface_name);
        if (!button_interface) {
            CMN_LOG_CLASS_INIT_ERROR << "Configure: failed to create button interface \""
                                     << interface_name << "\"" << std::endl;
            m_buttons.pop_back();
            continue;
        }
        button_interface->AddEventWrite(button.function, "Button", prmEventButton());
    }

    m_interface = AddInterfaceProvided(m_config.name);
    if (!m_interface) {
        CMN_LOG_CLASS_INIT_ERROR << "Configure: failed to create device interface \""
                                 << m_config.name << "\"" << std::endl;
        return;
    }

    m_interface->AddMessageEvents();
    m_interface->AddCommandReadState(StateTable, m_config.base_frame, "base_frame");
    m_interface->AddCommandReadState(StateTable, m_local_measured_cs, "local/measured_cs");
    m_interface->AddCommandFilteredReadState(StateTable, m_local_measured_cs,
                                             prmStateCartesian::ToPositionCartesianGet,
                                             "local/measured_cp");
    m_interface->AddCommandFilteredReadState(StateTable, m_local_measured_cs,
                                             prmStateCartesian::ToVelocityCartesianGet,
                                             "local/measured_cv");
    m_interface->AddCommandReadState(StateTable, m_measured_cs, "measured_cs");
    m_interface->AddCommandFilteredReadState(StateTable, m_measured_cs,
                                             prmStateCartesian::ToPositionCartesianGet,
                                             "measured_cp");
    m_interface->AddCommandFilteredReadState(StateTable, m_measured_cs,
                                             prmStateCartesian::ToVelocityCartesianGet,
                                             "measured_cv");
    m_interface->AddCommandFilteredReadState(StateTable, m_measured_cs,
                                             prmStateCartesian::ToPositionCartesianGet,
                                             "setpoint_cp");
    if (m_config.gripper.enabled) {
        m_interface->AddCommandReadState(StateTable, m_gripper_measured_js,
                                         "gripper/measured_js");
        m_interface->AddCommandRead(&mts3Dconnexion::configuration_js,
                                    this, "gripper/configuration_js");
    }
    m_interface->AddCommandRead(&mts3Dconnexion::GetButtonNames,
                                this, "get_button_names");
    m_interface->AddCommandReadState(StateTable, m_operating_state,
                                     "operating_state");
    m_interface->AddCommandWrite(&mts3Dconnexion::state_command,
                                 this, "state_command", std::string(""));
    m_interface->AddCommandWrite(&mts3Dconnexion::lock_orientation,
                                 this, "lock_orientation", false);
    m_interface->AddCommandWrite(&mts3Dconnexion::lock_position,
                                 this, "lock_position", false);
    m_interface->AddCommandWrite(&mts3Dconnexion::set_base_frame,
                                 this, "set_base_frame");
    m_interface->AddCommandVoid(&mts3Dconnexion::reset_orientation,
                                this, "reset_orientation");
    m_interface->AddCommandVoid(&mts3Dconnexion::reset_position,
                                this, "reset_position");
    m_interface->AddEventWrite(m_operating_state_event, "operating_state",
                               prmOperatingState());
    m_interface->AddEventWrite(m_orientation_locked_event, "orientation_locked", false);
    m_interface->AddEventWrite(m_position_locked_event, "position_locked", false);
    m_interface->AddCommandReadState(StateTable, StateTable.PeriodStats,
                                     "period_statistics");
}

void mts3Dconnexion::Configure(const std::string & _filename)
{
    if (m_configured) {
        CMN_LOG_CLASS_INIT_VERBOSE << "Configure: already configured" << std::endl;
        return;
    }

    if (hid_init()) {
        CMN_LOG_CLASS_INIT_ERROR << "Configure: hid_init failed" << std::endl;
        return;
    }

    if (_filename != "") {
        std::ifstream json_stream(_filename.c_str());
        if (!json_stream.is_open()) {
            CMN_LOG_CLASS_INIT_ERROR << "Configure: failed to open configuration file \""
                                     << _filename << "\"" << std::endl;
            return;
        }
        Json::Value json_config;
        Json::Reader json_reader;
        if (!json_reader.parse(json_stream, json_config)) {
            CMN_LOG_CLASS_INIT_ERROR << "Configure: failed to parse configuration file \""
                                     << _filename << "\"\n"
                                     << json_reader.getFormattedErrorMessages();
            return;
        }
        mtsComponent::ConfigureJSON(json_config);
        try {
            cmnDataDeSerializeTextJSON(m_config, json_config);
        } catch (std::exception & _exception) {
            CMN_LOG_CLASS_INIT_ERROR << "Configure: failed to deserialize configuration file \""
                                     << _filename << "\" (" << _exception.what() << ")"
                                     << std::endl;
            return;
        }
        CMN_LOG_CLASS_INIT_VERBOSE << "Configure, loaded:" << std::endl
                                   << "------>" << std::endl
                                   << cmnDataSerializeTextJSON(m_config) << std::endl
                                   << "<------" << std::endl;
    } else {
        CMN_LOG_CLASS_INIT_WARNING
            << "Configure: no JSON configuration file provided, using built-in defaults"
            << std::endl;
        m_config.name = "three_dconnexion";
    }

    if (!ApplyConfiguration()) {
        return;
    }

    ConfigureInterface();
    m_configured = (m_interface != nullptr);
    if (!m_configured) {
        return;
    }

    OpenDevice();
    if (!m_handle) {
        CMN_LOG_CLASS_INIT_ERROR << "Configure: failed to open 3Dconnexion device for interface \""
                                 << m_config.name << "\" using "
                                 << DeviceSearchDescription() << std::endl;
        CMN_LOG_CLASS_INIT_ERROR << "Configure: if a matching device is listed below, "
                                 << "verify /dev/hidraw permissions or install a udev rule"
                                 << std::endl;
        EnumerateDevices();
        return;
    }
}

bool mts3Dconnexion::IsConfigured(void) const
{
    return m_configured;
}

std::string mts3Dconnexion::GetDeviceName(void) const
{
    return m_config.name;
}

void mts3Dconnexion::Startup(void)
{
    if (m_handle) {
        hid_set_nonblocking(m_handle, 0);
        if (m_interface) {
            m_interface->SendStatus(m_config.name + ": 3Dconnexion device initialized");
        }
    }

    m_last_time = mtsManagerLocal::GetInstance()->GetTimeServer().GetRelativeTime();
    StateTable.Start();
    update_measured_cs();
    StateTable.Advance();
    if (m_operating_state_event.IsValid()) {
        m_operating_state_event(m_operating_state);
    }
    if (m_orientation_locked_event.IsValid()) {
        m_orientation_locked_event(m_config.lock_orientation);
    }
    if (m_position_locked_event.IsValid()) {
        m_position_locked_event(m_config.lock_position);
    }
}

void mts3Dconnexion::Run(void)
{
    StateTable.Start();
    if (m_interface) {
        m_interface->ProcessMailBoxes();
    }

    const double now = mtsManagerLocal::GetInstance()->GetTimeServer().GetRelativeTime();
    double period = now - m_last_time;
    if ((period < 0.0) || (period > m_config.max_integration_step)) {
        period = m_config.max_integration_step;
    }
    m_last_time = now;

    PollReports();
    IntegrateVirtualPose(period);
    StateTable.Advance();
}

void mts3Dconnexion::Cleanup(void)
{
    if (m_handle) {
        hid_close(m_handle);
        m_handle = nullptr;
    }
}

void mts3Dconnexion::update_measured_cs(void)
{
    if (!m_config.base_frame.ValidDefinition() || !m_config.base_frame.Fixed()) {
        m_measured_cs = m_local_measured_cs;
        m_measured_cs.PositionIsValid() = false;
        m_measured_cs.VelocityIsValid() = false;
        m_measured_cs.ForceIsValid() = false;
        return;
    }

    try {
        m_config.base_frame.ApplyTo(m_local_measured_cs, m_measured_cs);
    } catch (std::exception & _exception) {
        CMN_LOG_CLASS_RUN_ERROR << "update_measured_cs: failed to apply base_frame ("
                                << _exception.what() << ")" << std::endl;
        m_measured_cs = m_local_measured_cs;
        m_measured_cs.PositionIsValid() = false;
        m_measured_cs.VelocityIsValid() = false;
        m_measured_cs.ForceIsValid() = false;
    }
}

void mts3Dconnexion::set_base_frame(const prmPositionCartesianSet & _base_frame)
{
    if (_base_frame.Valid()) {
        m_config.base_frame.reference_frame() = _base_frame.ReferenceFrame().empty()
            ? std::string("user")
            : _base_frame.ReferenceFrame();
        m_config.base_frame.transform().FromNormalized(_base_frame.Goal());
    } else {
        m_config.base_frame.reference_frame().clear();
        m_config.base_frame.transform().Assign(vctFrm4x4::Identity());
    }
    update_measured_cs();
}

void mts3Dconnexion::PollReports(void)
{
    if (!m_handle) {
        m_local_measured_cs.PositionIsValid() = false;
        m_local_measured_cs.VelocityIsValid() = false;
        m_measured_cs.PositionIsValid() = false;
        m_measured_cs.VelocityIsValid() = false;
        if (!m_sent_connection_error && m_interface) {
            m_interface->SendError(m_config.name + ": no HID handle available");
            m_sent_connection_error = true;
        }
        return;
    }

    unsigned char buffer[256];
    for (size_t report_index = 0; report_index < m_config.max_reports_per_run; ++report_index) {
        const int timeout = (report_index == 0)
            ? static_cast<int>(m_config.read_timeout_ms)
            : 0;
        const int result = hid_read_timeout(m_handle, buffer, sizeof(buffer), timeout);
        if (result == 0) {
            break;
        }
        if (result < 0) {
            m_local_measured_cs.PositionIsValid() = false;
            m_local_measured_cs.VelocityIsValid() = false;
            m_measured_cs.PositionIsValid() = false;
            m_measured_cs.VelocityIsValid() = false;
            if (m_interface) {
                m_interface->SendError(m_config.name + ": failed reading HID report");
            }
            hid_close(m_handle);
            m_handle = nullptr;
            break;
        }
        ProcessReport(buffer, result);
    }
}

void mts3Dconnexion::ProcessReport(const unsigned char * _report,
                                   const int _length)
{
    if (_length <= 0) {
        return;
    }

    const int report_id = _report[0];
    if (m_config.reports.translation.enabled
        && (report_id == m_config.reports.translation.id)) {
        ProcessMotionReport(_report, _length, true);
    } else if (m_config.reports.rotation.enabled
               && (report_id == m_config.reports.rotation.id)) {
        ProcessMotionReport(_report, _length, false);
    } else if (m_config.reports.buttons.enabled
               && (report_id == m_config.reports.buttons.id)) {
        ProcessButtonReport(_report, _length);
    }
}

void mts3Dconnexion::ProcessMotionReport(const unsigned char * _report,
                                         const int _length,
                                         const bool _linear)
{
    const mts3DconnexionReportConfiguration & report_configuration = _linear
        ? m_config.reports.translation
        : m_config.reports.rotation;
    const mts3DconnexionAxisConfiguration & axis_configuration = _linear
        ? m_config.linear
        : m_config.angular;
    vct3 & raw_values = _linear ? m_raw_linear : m_raw_angular;
    const size_t velocity_offset = _linear ? 0 : 3;

    if (_length < static_cast<int>(report_configuration.offset + 6)) {
        return;
    }

    std::array<double, 3> raw;
    raw[0] = static_cast<double>(ReadInt16LE(_report, report_configuration.offset + 0));
    raw[1] = static_cast<double>(ReadInt16LE(_report, report_configuration.offset + 2));
    raw[2] = static_cast<double>(ReadInt16LE(_report, report_configuration.offset + 4));

    for (size_t target = 0; target < 3; ++target) {
        const int source = axis_configuration.axis_map[target];
        const double raw_value = ((source >= 0) && (source < 3)) ? raw[source] : 0.0;
        raw_values[target] = raw_value;
        double velocity =
            axis_configuration.sign[target]
            * ApplyDeadbandAndScale(raw_value,
                                    axis_configuration.full_scale,
                                    axis_configuration.deadband,
                                    axis_configuration.scale[target]);
        if ((_linear && m_config.lock_position)
            || (!_linear && m_config.lock_orientation)) {
            velocity = 0.0;
        }
        m_local_measured_cs.Velocity()[velocity_offset + target] = velocity;
    }
    m_local_measured_cs.VelocityIsValid() = true;
}

void mts3Dconnexion::ProcessButtonReport(const unsigned char * _report,
                                         const int _length)
{
    if (_length <= static_cast<int>(m_config.reports.buttons.offset)) {
        return;
    }

    uint32_t mask = 0;
    const size_t available_bytes =
        static_cast<size_t>(_length) - m_config.reports.buttons.offset;
    const size_t bytes =
        std::min<size_t>(std::min<size_t>(m_config.reports.buttons.bytes,
                                          available_bytes),
                         4);
    for (size_t index = 0; index < bytes; ++index) {
        mask |= static_cast<uint32_t>(_report[m_config.reports.buttons.offset + index])
            << (8 * index);
    }

    for (auto & button : m_buttons) {
        const bool pressed = (button.index < 32) && ((mask & (1u << button.index)) != 0);
        if (button.index >= m_button_states.size()) {
            m_button_states.resize(button.index + 1, false);
        }
        m_button_states[button.index] = pressed;
        if (button.pressed != pressed) {
            button.pressed = pressed;
            if (!button.suppress_events) {
                prmEventButton event;
                event.SetValid(true);
                event.SetTimestamp(StateTable.GetTic());
                event.SetType(pressed ? prmEventButton::PRESSED : prmEventButton::RELEASED);
                button.function(event);
            }
        }
    }
}

void mts3Dconnexion::IntegrateVirtualPose(const double & _period)
{
    for (size_t index = 0; index < 3; ++index) {
        m_local_measured_cs.Position().Translation()[index]
            += _period * m_local_measured_cs.Velocity()[index];
    }

    vct3 angular_velocity;
    for (size_t index = 0; index < 3; ++index) {
        angular_velocity[index] = m_local_measured_cs.Velocity()[index + 3];
    }
    const double angular_speed = angular_velocity.Norm();
    if (angular_speed > 1.0e-9) {
        const vct3 axis = angular_velocity / angular_speed;
        vctRot3 delta(vctAxAnRot3(axis, angular_speed * _period));
        m_local_measured_cs.Position().Rotation()
            = delta * m_local_measured_cs.Position().Rotation();
        m_local_measured_cs.Position().Rotation().NormalizedSelf();
    }
    if (m_local_measured_cs.VelocityIsValid()) {
        m_local_measured_cs.PositionIsValid() = true;
        update_measured_cs();
    } else {
        m_local_measured_cs.PositionIsValid() = false;
        m_measured_cs.PositionIsValid() = false;
        m_measured_cs.VelocityIsValid() = false;
    }

    if (m_config.gripper.enabled) {
        double gripper_velocity = 0.0;
        const bool open_pressed = ButtonState(m_config.gripper.open_button);
        const bool close_pressed = ButtonState(m_config.gripper.close_button);
        if (open_pressed != close_pressed) {
            gripper_velocity = (open_pressed ? 1.0 : -1.0) * m_config.gripper.rate;
        }
        double position = m_gripper_measured_js.Position()[0] + _period * gripper_velocity;
        position = std::max(m_config.gripper.min, std::min(m_config.gripper.max, position));
        m_gripper_measured_js.Position()[0] = position;
        m_gripper_measured_js.Velocity()[0] = gripper_velocity;
        m_gripper_measured_js.SetValid(true);
    }
}

bool mts3Dconnexion::ButtonState(const unsigned int & _index) const
{
    if (_index >= m_button_states.size()) {
        return false;
    }
    return m_button_states[_index];
}

void mts3Dconnexion::GetButtonNames(std::list<std::string> & _result) const
{
    _result.clear();
    for (const auto & button : m_buttons) {
        _result.push_back(button.name);
    }
}

void mts3Dconnexion::configuration_js(prmConfigurationJoint & _configuration) const
{
    _configuration = m_gripper_configuration_js;
}

void mts3Dconnexion::state_command(const std::string & _command)
{
    std::string human_readable_message;
    prmOperatingState::StateType new_operating_state;
    try {
        if (m_operating_state.ValidCommand(prmOperatingState::CommandTypeFromString(_command),
                                           new_operating_state,
                                           human_readable_message)) {
            if (_command == "enable") {
                m_operating_state.State() = prmOperatingState::ENABLED;
            } else if (_command == "disable") {
                m_operating_state.State() = prmOperatingState::DISABLED;
            } else if (_command == "home") {
                m_operating_state.IsHomed() = true;
                reset_pose();
            } else {
                m_interface->SendStatus(m_config.name + ": state command \""
                                        + _command + "\" is not supported yet");
            }
            m_operating_state.Valid() = true;
            m_operating_state_event(m_operating_state);
        } else {
            m_interface->SendWarning(m_config.name + ": " + human_readable_message);
        }
    } catch (std::runtime_error & _exception) {
        m_interface->SendWarning(m_config.name + ": " + _command
                                 + " doesn't seem to be a valid state_command ("
                                 + _exception.what() + ")");
    }
}

void mts3Dconnexion::lock_orientation(const bool & _lock)
{
    m_config.lock_orientation = _lock;
    for (size_t index = 0; index < 3; ++index) {
        m_local_measured_cs.Velocity()[index + 3] = 0.0;
    }
    update_measured_cs();
    if (m_orientation_locked_event.IsValid()) {
        m_orientation_locked_event(m_config.lock_orientation);
    }
}

void mts3Dconnexion::lock_position(const bool & _lock)
{
    m_config.lock_position = _lock;
    for (size_t index = 0; index < 3; ++index) {
        m_local_measured_cs.Velocity()[index] = 0.0;
    }
    update_measured_cs();
    if (m_position_locked_event.IsValid()) {
        m_position_locked_event(m_config.lock_position);
    }
}

void mts3Dconnexion::reset_pose(void)
{
    m_local_measured_cs.Position().Assign(vctFrm3::Identity());
    m_local_measured_cs.PositionIsValid() = true;
    update_measured_cs();
}

void mts3Dconnexion::reset_orientation(void)
{
    m_local_measured_cs.Position().Rotation().Assign(vctRot3::Identity());
    update_measured_cs();
}

void mts3Dconnexion::reset_position(void)
{
    m_local_measured_cs.Position().Translation().Assign(vct3(0.0));
    update_measured_cs();
}
