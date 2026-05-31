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

#include <cisstMultiTask/mtsFunctionWrite.h>
#include <cisstMultiTask/mtsFunctionRead.h>
#include <cisstMultiTask/mtsInterfaceProvided.h>
#include <cisstMultiTask/mtsInterfaceRequired.h>
#include <cisstMultiTask/mtsManagerLocal.h>
#include <cisstParameterTypes/prmBaseFrame.h>
#include <cisstParameterTypes/prmConfigurationJoint.h>
#include <cisstParameterTypes/prmEventButton.h>
#include <cisstParameterTypes/prmOperatingState.h>
#include <cisstParameterTypes/prmPositionCartesianGet.h>
#include <cisstParameterTypes/prmPositionCartesianSet.h>
#include <cisstParameterTypes/prmStateCartesian.h>
#include <cisstParameterTypes/prmStateJoint.h>
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
#include <vector>

CMN_IMPLEMENT_SERVICES_DERIVED_ONEARG(mts3Dconnexion, mtsTaskContinuous, mtsTaskContinuousConstructorArg);

namespace {

struct ReportConfiguration {
    int Id;
    size_t Offset;
    size_t Bytes;
    bool Enabled;

    ReportConfiguration(const int id = 0,
                        const size_t offset = 1,
                        const size_t bytes = 6):
        Id(id),
        Offset(offset),
        Bytes(bytes),
        Enabled(true)
    {}
};

struct AxisConfiguration {
    std::array<double, 3> Scale;
    std::array<double, 3> Sign;
    std::array<int, 3> Map;
    double FullScale;
    double Deadband;

    AxisConfiguration():
        Scale{{0.05, 0.05, 0.05}},
        Sign{{1.0, 1.0, 1.0}},
        Map{{0, 1, 2}},
        FullScale(350.0),
        Deadband(0.02)
    {}
};

struct ButtonConfiguration {
    unsigned int Index;
    std::string Name;
    bool SuppressEvents;

    ButtonConfiguration(const unsigned int index = 0,
                        const std::string & name = "",
                        const bool suppressEvents = false):
        Index(index),
        Name(name),
        SuppressEvents(suppressEvents)
    {}
};

struct GripperConfiguration {
    bool Provide;
    bool Emulate;
    unsigned int OpenButton;
    unsigned int CloseButton;
    double Min;
    double Max;
    double Initial;
    double Rate;

    GripperConfiguration():
        Provide(true),
        Emulate(true),
        OpenButton(1),
        CloseButton(0),
        Min(0.0),
        Max(1.0),
        Initial(1.0),
        Rate(1.0)
    {}
};

static std::string WideToString(const wchar_t * value)
{
    if (!value) {
        return "";
    }
    std::wstring wide(value);
    return std::string(wide.begin(), wide.end());
}

static std::wstring StringToWide(const std::string & value)
{
    return std::wstring(value.begin(), value.end());
}

static unsigned short JsonToUnsignedShort(const Json::Value & value,
                                          const unsigned short defaultValue)
{
    if (value.empty()) {
        return defaultValue;
    }
    if (value.isString()) {
        const std::string text = value.asString();
        const int base = (text.find("0x") == 0 || text.find("0X") == 0) ? 0 : 16;
        return static_cast<unsigned short>(std::stoul(text, nullptr, base));
    }
    return static_cast<unsigned short>(value.asUInt());
}

static void ReadDoubleArray3(const Json::Value & value,
                             std::array<double, 3> & output)
{
    if (value.empty()) {
        return;
    }
    if (value.isArray()) {
        for (unsigned int index = 0; index < value.size() && index < 3; ++index) {
            output[index] = value[index].asDouble();
        }
    } else {
        output[0] = value.asDouble();
        output[1] = output[0];
        output[2] = output[0];
    }
}

static void ReadIntArray3(const Json::Value & value,
                          std::array<int, 3> & output)
{
    if (value.empty()) {
        return;
    }
    for (unsigned int index = 0; index < value.size() && index < 3; ++index) {
        output[index] = value[index].asInt();
    }
}

static void ConfigureAxis(const Json::Value & value,
                          AxisConfiguration & axis)
{
    if (value.empty()) {
        return;
    }
    ReadDoubleArray3(value["scale"], axis.Scale);
    ReadDoubleArray3(value["sign"], axis.Sign);
    ReadIntArray3(value["axis_map"], axis.Map);
    if (!value["full_scale"].empty()) {
        axis.FullScale = value["full_scale"].asDouble();
    }
    if (!value["deadband"].empty()) {
        axis.Deadband = value["deadband"].asDouble();
    }
}

static void ConfigureReport(const Json::Value & value,
                            ReportConfiguration & report)
{
    if (value.empty()) {
        return;
    }
    if (!value["id"].empty()) {
        report.Id = value["id"].asInt();
    }
    if (!value["offset"].empty()) {
        report.Offset = value["offset"].asUInt();
    }
    if (!value["bytes"].empty()) {
        report.Bytes = value["bytes"].asUInt();
    }
    if (!value["enabled"].empty()) {
        report.Enabled = value["enabled"].asBool();
    }
}

static double ApplyDeadbandAndScale(const double raw,
                                    const double fullScale,
                                    const double deadband,
                                    const double scale)
{
    if (fullScale <= 0.0) {
        return 0.0;
    }
    double normalized = raw / fullScale;
    normalized = std::max(-1.0, std::min(1.0, normalized));
    const double magnitude = std::abs(normalized);
    if (magnitude <= deadband) {
        return 0.0;
    }
    const double adjustedMagnitude = (magnitude - deadband)
                                     / std::max(1.0e-6, 1.0 - deadband);
    return std::copysign(adjustedMagnitude * scale, normalized);
}

static int16_t ReadInt16LE(const unsigned char * data,
                           const size_t offset)
{
    const uint16_t value = static_cast<uint16_t>(data[offset])
                           | (static_cast<uint16_t>(data[offset + 1]) << 8);
    return static_cast<int16_t>(value);
}

static std::string IdToString(const unsigned short value)
{
    std::stringstream stream;
    stream << "0x" << std::hex << std::setw(4) << std::setfill('0') << value;
    return stream.str();
}

static void CopyStateToCartesianReads(const prmStateCartesian & state,
                                      prmPositionCartesianGet & measuredCp,
                                      prmPositionCartesianGet & setpointCp,
                                      prmVelocityCartesianGet & measuredCv)
{
    measuredCp.SetReferenceFrame(state.ReferenceFrame());
    measuredCp.SetMovingFrame(state.MovingFrame());
    measuredCp.Position().Assign(state.Position());
    measuredCp.SetValid(state.PositionIsValid());

    setpointCp.SetReferenceFrame(state.ReferenceFrame());
    setpointCp.SetMovingFrame(state.MovingFrame());
    setpointCp.Position().Assign(state.Position());
    setpointCp.SetValid(state.PositionIsValid());

    measuredCv.SetReferenceFrame(state.ReferenceFrame());
    measuredCv.SetMovingFrame(state.MovingFrame());
    for (size_t index = 0; index < 3; ++index) {
        measuredCv.VelocityLinear()[index] = state.Velocity()[index];
        measuredCv.VelocityAngular()[index] = state.Velocity()[index + 3];
    }
    measuredCv.SetValid(state.VelocityIsValid());
}

} // namespace

class mts3DconnexionData
{
public:
    struct ButtonData {
        unsigned int Index;
        std::string Name;
        bool SuppressEvents;
        bool Pressed;
        mtsFunctionWrite Function;
    };

    typedef std::list<ButtonData *> ButtonsData;

    mts3DconnexionData(void):
        Configured(false),
        InterfaceName("MTMR"),
        NativeReferenceFrame("MTMR_base"),
        NativeMovingFrame("MTMR"),
        VendorId(0x256f),
        ProductId(0),
        MatchProductId(false),
        LockOrientation(false),
        LockPosition(false),
        ReadTimeoutMs(1),
        MaxReportsPerRun(32),
        MaxIntegrationStep(0.05),
        TranslationReport(1, 1, 6),
        RotationReport(2, 1, 6),
        ButtonReport(3, 1, 4),
        Handle(nullptr),
        Interface(nullptr),
        BaseFrameInterface(nullptr),
        LastTime(0.0),
        SentConnectionError(false),
        BaseFrameErrorSent(false),
        BaseFrameResolved(false),
        ButtonStates(64, false)
    {
        ReferenceFrame = NativeReferenceFrame;
        MovingFrame = NativeMovingFrame;
        ButtonConfigurations.push_back(ButtonConfiguration(0, "left"));
        ButtonConfigurations.push_back(ButtonConfiguration(1, "right"));
        RawLinear.Zeros();
        RawAngular.Zeros();
    }

    ~mts3DconnexionData(void) {
        for (auto button : Buttons) {
            delete button;
        }
        Buttons.clear();
    }

    bool Configured;
    std::string InterfaceName;
    std::string NativeReferenceFrame;
    std::string NativeMovingFrame;
    unsigned short VendorId;
    unsigned short ProductId;
    bool MatchProductId;
    bool LockOrientation;
    bool LockPosition;
    std::string Serial;
    std::string Path;
    unsigned int ReadTimeoutMs;
    std::string ReferenceFrame;
    std::string MovingFrame;
    prmBaseFrame BaseFrame;
    size_t MaxReportsPerRun;
    double MaxIntegrationStep;
    ReportConfiguration TranslationReport;
    ReportConfiguration RotationReport;
    ReportConfiguration ButtonReport;
    AxisConfiguration Linear;
    AxisConfiguration Angular;
    GripperConfiguration Gripper;
    std::vector<ButtonConfiguration> ButtonConfigurations;

    hid_device * Handle;
    mtsInterfaceProvided * Interface;
    mtsInterfaceRequired * BaseFrameInterface;
    mtsFunctionRead BaseFrameMeasuredCp;
    double LastTime;
    bool SentConnectionError;
    bool BaseFrameErrorSent;
    bool BaseFrameResolved;

    prmOperatingState OperatingState;
    mtsFunctionWrite OperatingStateEvent;
    mtsFunctionWrite OrientationLockedEvent;
    mtsFunctionWrite PositionLockedEvent;
    prmPositionCartesianGet LocalMeasuredCp;
    prmPositionCartesianGet MeasuredCp;
    prmPositionCartesianGet SetpointCp;
    prmVelocityCartesianGet LocalMeasuredCv;
    prmVelocityCartesianGet MeasuredCv;
    prmPositionCartesianGet ExternalBaseFrameCp;
    prmStateCartesian LocalMeasuredCs;
    prmStateCartesian MeasuredCs;
    prmStateJoint GripperMeasuredJs;
    prmConfigurationJoint GripperConfigurationJs;

    ButtonsData Buttons;
    std::vector<bool> ButtonStates;
    vct3 RawLinear;
    vct3 RawAngular;
};

static bool DeviceMatches(const mts3DconnexionData & data,
                          const hid_device_info * device)
{
    if (!device) {
        return false;
    }
    const bool vendorMatches = device->vendor_id == data.VendorId;
    const bool productMatches = (!data.MatchProductId)
                                || (device->product_id == data.ProductId);
    const bool serialMatches = data.Serial.empty()
                               || (WideToString(device->serial_number) == data.Serial);
    return vendorMatches && productMatches && serialMatches;
}

static std::string DeviceSearchDescription(const mts3DconnexionData & data)
{
    std::stringstream stream;
    stream << "vendor " << IdToString(data.VendorId);
    if (data.MatchProductId) {
        stream << ", product " << IdToString(data.ProductId);
    }
    if (!data.Serial.empty()) {
        stream << ", serial \"" << data.Serial << "\"";
    }
    if (!data.Path.empty()) {
        stream << ", path \"" << data.Path << "\"";
    }
    return stream.str();
}

static bool ParseConfiguration(const Json::Value & jsonConfig,
                               mts3DconnexionData & data)
{
    const bool interfaceProvided = !jsonConfig["interface-name"].empty()
                                   || !jsonConfig["interface"].empty()
                                   || !jsonConfig["name"].empty();
    if (!jsonConfig["interface-name"].empty()) {
        data.InterfaceName = jsonConfig["interface-name"].asString();
    } else if (!jsonConfig["interface"].empty()) {
        data.InterfaceName = jsonConfig["interface"].asString();
    } else if (!jsonConfig["name"].empty()) {
        data.InterfaceName = jsonConfig["name"].asString();
    }
    if (interfaceProvided) {
        data.NativeReferenceFrame = data.InterfaceName + "_base";
        data.NativeMovingFrame = data.InterfaceName;
        data.ReferenceFrame = data.NativeReferenceFrame;
        data.MovingFrame = data.NativeMovingFrame;
    }

    if (!jsonConfig["vendor_id"].empty()) {
        data.VendorId = JsonToUnsignedShort(jsonConfig["vendor_id"], data.VendorId);
    } else if (!jsonConfig["id_vendor"].empty()) {
        data.VendorId = JsonToUnsignedShort(jsonConfig["id_vendor"], data.VendorId);
    }
    if (!jsonConfig["product_id"].empty()) {
        data.ProductId = JsonToUnsignedShort(jsonConfig["product_id"], data.ProductId);
        data.MatchProductId = true;
    } else if (!jsonConfig["id_product"].empty()) {
        data.ProductId = JsonToUnsignedShort(jsonConfig["id_product"], data.ProductId);
        data.MatchProductId = true;
    }
    if (!jsonConfig["serial"].empty()) {
        data.Serial = jsonConfig["serial"].asString();
    }
    if (!jsonConfig["path"].empty()) {
        data.Path = jsonConfig["path"].asString();
    }
    if (!jsonConfig["lock_orientation"].empty()) {
        data.LockOrientation = jsonConfig["lock_orientation"].asBool();
    }
    if (!jsonConfig["lock_position"].empty()) {
        data.LockPosition = jsonConfig["lock_position"].asBool();
    }
    if (!jsonConfig["read_timeout_ms"].empty()) {
        data.ReadTimeoutMs = jsonConfig["read_timeout_ms"].asUInt();
    }
    if (!jsonConfig["reference_frame"].empty()) {
        data.ReferenceFrame = jsonConfig["reference_frame"].asString();
    }
    if (!jsonConfig["moving_frame"].empty()) {
        data.MovingFrame = jsonConfig["moving_frame"].asString();
    }
    if (!jsonConfig["base_frame"].empty()) {
        data.BaseFrame.DeSerializeTextJSON(jsonConfig["base_frame"]);
        if (!data.BaseFrame.Valid()) {
            CMN_LOG_INIT_ERROR << "Configure: invalid \"base_frame\" syntax"
                               << std::endl;
            return false;
        }
    }
    if (data.BaseFrame.Fixed()) {
        data.ReferenceFrame = data.BaseFrame.reference_frame();
    }
    if (!jsonConfig["max_reports_per_run"].empty()) {
        data.MaxReportsPerRun = jsonConfig["max_reports_per_run"].asUInt();
    }
    if (!jsonConfig["max_integration_step"].empty()) {
        data.MaxIntegrationStep = jsonConfig["max_integration_step"].asDouble();
    }

    const Json::Value reports = jsonConfig["reports"];
    ConfigureReport(reports["translation"], data.TranslationReport);
    ConfigureReport(reports["rotation"], data.RotationReport);
    ConfigureReport(reports["buttons"], data.ButtonReport);
    if (!jsonConfig["translation_report"].empty()) {
        data.TranslationReport.Id = jsonConfig["translation_report"].asInt();
    }
    if (!jsonConfig["rotation_report"].empty()) {
        data.RotationReport.Id = jsonConfig["rotation_report"].asInt();
    }
    if (!jsonConfig["button_report"].empty()) {
        data.ButtonReport.Id = jsonConfig["button_report"].asInt();
    }

    ConfigureAxis(jsonConfig["linear"], data.Linear);
    ConfigureAxis(jsonConfig["angular"], data.Angular);
    ReadDoubleArray3(jsonConfig["linear_scale"], data.Linear.Scale);
    ReadDoubleArray3(jsonConfig["angular_scale"], data.Angular.Scale);
    if (!jsonConfig["raw_full_scale"].empty()) {
        data.Linear.FullScale = jsonConfig["raw_full_scale"].asDouble();
        data.Angular.FullScale = data.Linear.FullScale;
    }
    if (!jsonConfig["deadband"].empty()) {
        data.Linear.Deadband = jsonConfig["deadband"].asDouble();
        data.Angular.Deadband = data.Linear.Deadband;
    }
    ReadIntArray3(jsonConfig["linear_axis_map"], data.Linear.Map);
    ReadIntArray3(jsonConfig["angular_axis_map"], data.Angular.Map);
    ReadDoubleArray3(jsonConfig["linear_axis_sign"], data.Linear.Sign);
    ReadDoubleArray3(jsonConfig["angular_axis_sign"], data.Angular.Sign);

    const Json::Value buttons = jsonConfig["buttons"];
    if (jsonConfig.isMember("buttons")) {
        data.ButtonConfigurations.clear();
        for (unsigned int index = 0; index < buttons.size(); ++index) {
            ButtonConfiguration button;
            button.Index = buttons[index]["index"].asUInt();
            button.Name = buttons[index]["name"].asString();
            if (button.Name == "") {
                std::stringstream name;
                name << "button_" << button.Index;
                button.Name = name.str();
            }
            if (!buttons[index]["suppress_events"].empty()) {
                button.SuppressEvents = buttons[index]["suppress_events"].asBool();
            }
            data.ButtonConfigurations.push_back(button);
        }
    }

    const Json::Value gripper = jsonConfig["gripper"];
    if (!gripper.empty()) {
        if (!gripper["provide"].empty()) {
            data.Gripper.Provide = gripper["provide"].asBool();
        }
        if (!gripper["emulate"].empty()) {
            data.Gripper.Emulate = gripper["emulate"].asBool();
        }
        if (!gripper["open_button"].empty()) {
            data.Gripper.OpenButton = gripper["open_button"].asUInt();
        }
        if (!gripper["close_button"].empty()) {
            data.Gripper.CloseButton = gripper["close_button"].asUInt();
        }
        if (!gripper["min"].empty()) {
            data.Gripper.Min = gripper["min"].asDouble();
        }
        if (!gripper["max"].empty()) {
            data.Gripper.Max = gripper["max"].asDouble();
        }
        if (!gripper["initial"].empty()) {
            data.Gripper.Initial = gripper["initial"].asDouble();
        } else {
            data.Gripper.Initial = data.Gripper.Max;
        }
        if (!gripper["rate"].empty()) {
            data.Gripper.Rate = gripper["rate"].asDouble();
        }
    }

    return true;
}

static bool ResolveBaseFrame(mts3DconnexionData & data,
                             prmBaseFrame & resolvedBaseFrame)
{
    if (data.BaseFrame.Fixed()) {
        data.BaseFrameResolved = true;
        resolvedBaseFrame = data.BaseFrame;
        return true;
    }

    if (!data.BaseFrame.Dynamic()) {
        data.BaseFrameResolved = true;
        return false;
    }

    if (!data.BaseFrameMeasuredCp.IsValid()) {
        data.BaseFrameResolved = false;
        return false;
    }

    mtsExecutionResult executionResult = data.BaseFrameMeasuredCp(data.ExternalBaseFrameCp);
    if (!executionResult.IsOK() || !data.ExternalBaseFrameCp.Valid()) {
        data.BaseFrameResolved = false;
        return false;
    }

    resolvedBaseFrame.reference_frame() = data.ExternalBaseFrameCp.ReferenceFrame();
    resolvedBaseFrame.transform().From(data.ExternalBaseFrameCp.Position());
    resolvedBaseFrame.component() = "";
    resolvedBaseFrame.interface() = "";
    data.BaseFrameResolved = true;
    return true;
}

static void ApplyBaseFrameToState(mts3DconnexionData & data)
{
    CopyStateToCartesianReads(data.LocalMeasuredCs,
                              data.LocalMeasuredCp,
                              data.LocalMeasuredCp,
                              data.LocalMeasuredCv);

    data.MeasuredCs = data.LocalMeasuredCs;

    prmBaseFrame resolvedBaseFrame;
    if (data.BaseFrame.Fixed() || data.BaseFrame.Dynamic()) {
        if (!ResolveBaseFrame(data, resolvedBaseFrame)) {
            data.MeasuredCs.PositionIsValid() = false;
            data.MeasuredCs.VelocityIsValid() = false;
            CopyStateToCartesianReads(data.MeasuredCs,
                                      data.MeasuredCp,
                                      data.SetpointCp,
                                      data.MeasuredCv);
            return;
        }
        resolvedBaseFrame.ApplyTo(data.LocalMeasuredCs, data.MeasuredCs);
    }

    CopyStateToCartesianReads(data.MeasuredCs,
                              data.MeasuredCp,
                              data.SetpointCp,
                              data.MeasuredCv);
}

mts3Dconnexion::mts3Dconnexion(const std::string & componentName):
    mtsTaskContinuous(componentName, 256)
{
    Init();
}

mts3Dconnexion::mts3Dconnexion(const mtsTaskContinuousConstructorArg & arg):
    mtsTaskContinuous(arg)
{
    Init();
}

mts3Dconnexion::~mts3Dconnexion(void)
{
    Cleanup();
    delete m_data;
}

void mts3Dconnexion::Init(void)
{
    m_data = new mts3DconnexionData;
}

void mts3Dconnexion::EnumerateDevices(void) const
{
    struct hid_device_info * devs = hid_enumerate(0x0, 0x0);
    struct hid_device_info * current = devs;
    std::set<std::string> reportedDevices;
    size_t deviceCount = 0;
    size_t matchingDeviceCount = 0;
    while (current) {
        std::stringstream deviceKey;
        deviceKey << current->vendor_id << ":"
                  << current->product_id << ":"
                  << (current->path ? current->path : "") << ":"
                  << current->interface_number;
        if (!reportedDevices.insert(deviceKey.str()).second) {
            current = current->next;
            continue;
        }

        ++deviceCount;
        const bool matches = DeviceMatches(*m_data, current);
        if (matches) {
            ++matchingDeviceCount;
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
    if (deviceCount == 0) {
        CMN_LOG_CLASS_INIT_WARNING << "No HID devices visible to hidapi" << std::endl;
    } else if (matchingDeviceCount == 0) {
        CMN_LOG_CLASS_INIT_WARNING << "No HID devices matched "
                                   << DeviceSearchDescription(*m_data) << std::endl;
    }
    hid_free_enumeration(devs);
}

void mts3Dconnexion::OpenDevice(void)
{
    if (m_data->Path != "") {
        m_data->Handle = hid_open_path(m_data->Path.c_str());
        if (!m_data->Handle) {
            CMN_LOG_CLASS_INIT_WARNING
                << "OpenDevice: failed to open configured HID path \""
                << m_data->Path << "\"" << std::endl;
        }
        return;
    }

    struct hid_device_info * devs = hid_enumerate(m_data->VendorId,
                                                 m_data->MatchProductId ? m_data->ProductId : 0x0);
    struct hid_device_info * current = devs;
    std::set<std::string> triedPaths;
    while (current) {
        if (DeviceMatches(*m_data, current) && current->path) {
            if (!triedPaths.insert(current->path).second) {
                current = current->next;
                continue;
            }
            m_data->Handle = hid_open_path(current->path);
            if (m_data->Handle) {
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

    if (!m_data->Handle && m_data->MatchProductId) {
        if (m_data->Serial.empty()) {
            m_data->Handle = hid_open(m_data->VendorId, m_data->ProductId, nullptr);
        } else {
            const std::wstring serial = StringToWide(m_data->Serial);
            m_data->Handle = hid_open(m_data->VendorId, m_data->ProductId, serial.c_str());
        }
    }
}

void mts3Dconnexion::ConfigureInterface(void)
{
    m_data->OperatingState.IsBusy() = false;
    m_data->OperatingState.IsHomed() = true;
    m_data->OperatingState.State() = prmOperatingState::ENABLED;
    m_data->OperatingState.Valid() = true;

    if (m_data->BaseFrame.Dynamic()) {
        m_data->BaseFrameInterface = AddInterfaceRequired("BaseFrame");
        if (!m_data->BaseFrameInterface) {
            CMN_LOG_CLASS_INIT_ERROR << "Configure: failed to create required interface \"BaseFrame\""
                                     << std::endl;
            return;
        }
        m_data->BaseFrameInterface->AddFunction("measured_cp", m_data->BaseFrameMeasuredCp);
    }

    m_data->LocalMeasuredCs.MovingFrame() = m_data->MovingFrame;
    m_data->LocalMeasuredCs.ReferenceFrame() = m_data->NativeReferenceFrame;
    m_data->LocalMeasuredCs.Position().Assign(vctFrm3::Identity());
    m_data->LocalMeasuredCs.PositionIsValid() = true;
    m_data->LocalMeasuredCs.Velocity().Assign(vct6(0.0));
    m_data->LocalMeasuredCs.VelocityIsValid() = true;
    m_data->LocalMeasuredCs.Force().Assign(vct6(0.0));
    m_data->LocalMeasuredCs.ForceIsValid() = false;

    m_data->MeasuredCs = m_data->LocalMeasuredCs;
    if (m_data->BaseFrame.Fixed()) {
        m_data->MeasuredCs.ReferenceFrame() = m_data->BaseFrame.reference_frame();
    }

    m_data->LocalMeasuredCp.SetReferenceFrame(m_data->NativeReferenceFrame);
    m_data->LocalMeasuredCp.SetMovingFrame(m_data->MovingFrame);
    m_data->LocalMeasuredCp.Position().Assign(vctFrm3::Identity());
    m_data->LocalMeasuredCp.SetValid(true);

    m_data->MeasuredCp.SetReferenceFrame(m_data->ReferenceFrame);
    m_data->MeasuredCp.SetMovingFrame(m_data->MovingFrame);
    m_data->MeasuredCp.Position().Assign(vctFrm3::Identity());
    m_data->MeasuredCp.SetValid(true);

    m_data->SetpointCp.SetReferenceFrame(m_data->ReferenceFrame);
    m_data->SetpointCp.SetMovingFrame(m_data->MovingFrame);
    m_data->SetpointCp.Position().Assign(vctFrm3::Identity());
    m_data->SetpointCp.SetValid(true);

    m_data->LocalMeasuredCv.SetReferenceFrame(m_data->NativeReferenceFrame);
    m_data->LocalMeasuredCv.SetMovingFrame(m_data->MovingFrame);
    m_data->LocalMeasuredCv.VelocityLinear().Zeros();
    m_data->LocalMeasuredCv.VelocityAngular().Zeros();
    m_data->LocalMeasuredCv.SetValid(true);

    m_data->MeasuredCv.SetReferenceFrame(m_data->ReferenceFrame);
    m_data->MeasuredCv.SetMovingFrame(m_data->MovingFrame);
    m_data->MeasuredCv.VelocityLinear().Zeros();
    m_data->MeasuredCv.VelocityAngular().Zeros();
    m_data->MeasuredCv.SetValid(true);

    m_data->GripperMeasuredJs.SetSize(1);
    m_data->GripperMeasuredJs.Name()[0] = "gripper";
    m_data->GripperMeasuredJs.Position()[0] = m_data->Gripper.Initial;
    m_data->GripperMeasuredJs.Velocity()[0] = 0.0;
    m_data->GripperMeasuredJs.Effort()[0] = 0.0;
    m_data->GripperMeasuredJs.SetValid(m_data->Gripper.Provide);

    m_data->GripperConfigurationJs.Name().resize(1);
    m_data->GripperConfigurationJs.Name()[0] = "gripper";
    m_data->GripperConfigurationJs.Type().SetSize(1);
    m_data->GripperConfigurationJs.Type()[0] = CMN_JOINT_REVOLUTE;
    m_data->GripperConfigurationJs.PositionMin().SetSize(1);
    m_data->GripperConfigurationJs.PositionMin()[0] = m_data->Gripper.Min;
    m_data->GripperConfigurationJs.PositionMax().SetSize(1);
    m_data->GripperConfigurationJs.PositionMax()[0] = m_data->Gripper.Max;

    StateTable.SetAutomaticAdvance(false);
    StateTable.AddData(m_data->OperatingState, "operating_state");
    StateTable.AddData(m_data->LocalMeasuredCp, "local/measured_cp");
    StateTable.AddData(m_data->MeasuredCp, "measured_cp");
    StateTable.AddData(m_data->LocalMeasuredCv, "local/measured_cv");
    StateTable.AddData(m_data->MeasuredCv, "measured_cv");
    StateTable.AddData(m_data->SetpointCp, "setpoint_cp");
    if (m_data->Gripper.Provide) {
        StateTable.AddData(m_data->GripperMeasuredJs, "gripper/measured_js");
    }

    for (const auto & buttonConfiguration : m_data->ButtonConfigurations) {
        mts3DconnexionData::ButtonData * data = new mts3DconnexionData::ButtonData;
        data->Index = buttonConfiguration.Index;
        data->Name = buttonConfiguration.Name;
        data->SuppressEvents = buttonConfiguration.SuppressEvents;
        data->Pressed = false;

        const std::string interfaceName = m_data->InterfaceName + "/" + data->Name;
        mtsInterfaceProvided * buttonInterface = AddInterfaceProvided(interfaceName);
        if (!buttonInterface) {
            CMN_LOG_CLASS_INIT_ERROR << "Configure: failed to create button interface \""
                                     << interfaceName << "\"" << std::endl;
            delete data;
            continue;
        }
        buttonInterface->AddEventWrite(data->Function, "Button", prmEventButton());
        m_data->Buttons.push_back(data);
    }

    m_data->Interface = AddInterfaceProvided(m_data->InterfaceName);
    if (!m_data->Interface) {
        CMN_LOG_CLASS_INIT_ERROR << "Configure: failed to create device interface \""
                                 << m_data->InterfaceName << "\"" << std::endl;
        return;
    }

    m_data->Interface->AddMessageEvents();
    m_data->Interface->AddCommandReadState(StateTable, m_data->LocalMeasuredCp,
                                           "local/measured_cp");
    m_data->Interface->AddCommandReadState(StateTable, m_data->MeasuredCp, "measured_cp");
    m_data->Interface->AddCommandReadState(StateTable, m_data->LocalMeasuredCv,
                                           "local/measured_cv");
    m_data->Interface->AddCommandReadState(StateTable, m_data->MeasuredCv, "measured_cv");
    m_data->Interface->AddCommandReadState(StateTable, m_data->SetpointCp, "setpoint_cp");
    if (m_data->Gripper.Provide) {
        m_data->Interface->AddCommandReadState(StateTable, m_data->GripperMeasuredJs,
                                               "gripper/measured_js");
        m_data->Interface->AddCommandRead(&mts3Dconnexion::GetConfigurationJs,
                                          this, "gripper/get_configuration_js");
    }
    m_data->Interface->AddCommandRead(&mts3Dconnexion::GetButtonNames,
                                      this, "get_button_names");
    m_data->Interface->AddCommandReadState(StateTable, m_data->OperatingState,
                                           "operating_state");
    m_data->Interface->AddCommandWrite(&mts3Dconnexion::state_command,
                                       this, "state_command", std::string(""));
    m_data->Interface->AddCommandWrite(&mts3Dconnexion::lock_orientation,
                                       this, "lock_orientation", false);
    m_data->Interface->AddCommandWrite(&mts3Dconnexion::lock_position,
                                       this, "lock_position", false);
    m_data->Interface->AddEventWrite(m_data->OperatingStateEvent, "operating_state",
                                     prmOperatingState());
    m_data->Interface->AddEventWrite(m_data->OrientationLockedEvent, "orientation_locked", false);
    m_data->Interface->AddEventWrite(m_data->PositionLockedEvent, "position_locked", false);
    m_data->Interface->AddCommandWrite(&mts3Dconnexion::move_cp, this, "move_cp");
    m_data->Interface->AddCommandReadState(StateTable, StateTable.PeriodStats,
                                           "period_statistics");
}

void mts3Dconnexion::Configure(const std::string & filename)
{
    if (m_data->Configured) {
        CMN_LOG_CLASS_INIT_VERBOSE << "Configure: already configured" << std::endl;
        return;
    }

    if (hid_init()) {
        CMN_LOG_CLASS_INIT_ERROR << "Configure: hid_init failed" << std::endl;
        return;
    }

    if (filename != "") {
        std::ifstream jsonStream(filename.c_str());
        if (!jsonStream.is_open()) {
            CMN_LOG_CLASS_INIT_ERROR << "Configure: failed to open configuration file \""
                                     << filename << "\"" << std::endl;
            return;
        }
        Json::Value jsonConfig;
        Json::Reader jsonReader;
        if (!jsonReader.parse(jsonStream, jsonConfig)) {
            CMN_LOG_CLASS_INIT_ERROR << "Configure: failed to parse configuration file \""
                                     << filename << "\"\n"
                                     << jsonReader.getFormattedErrorMessages();
            return;
        }
        mtsComponent::ConfigureJSON(jsonConfig);
        if (!ParseConfiguration(jsonConfig, *m_data)) {
            return;
        }
    } else {
        CMN_LOG_CLASS_INIT_WARNING
            << "Configure: no JSON configuration file provided, using built-in defaults"
            << std::endl;
    }

    ConfigureInterface();
    m_data->Configured = (m_data->Interface != nullptr);
    if (!m_data->Configured) {
        return;
    }

    OpenDevice();
    if (!m_data->Handle) {
        CMN_LOG_CLASS_INIT_ERROR << "Configure: failed to open 3Dconnexion device for interface \""
                                 << m_data->InterfaceName << "\" using "
                                 << DeviceSearchDescription(*m_data) << std::endl;
        CMN_LOG_CLASS_INIT_ERROR << "Configure: if a matching device is listed below, "
                                 << "verify /dev/hidraw permissions or install a udev rule"
                                 << std::endl;
        EnumerateDevices();
        return;
    }
}

bool mts3Dconnexion::IsConfigured(void) const
{
    return m_data->Configured;
}

void mts3Dconnexion::Startup(void)
{
    if (m_data->BaseFrame.Dynamic()) {
        mtsManagerLocal * componentManager = mtsManagerLocal::GetInstance();
        componentManager->Connect(m_data->BaseFrame.component(),
                                  m_data->BaseFrame.interface(),
                                  this->GetName(), "BaseFrame");
    }

    if (m_data->Handle) {
        hid_set_nonblocking(m_data->Handle, 0);
        if (m_data->Interface) {
            m_data->Interface->SendStatus(m_data->InterfaceName + ": 3Dconnexion device initialized");
        }
    }

    m_data->LastTime = mtsManagerLocal::GetInstance()->GetTimeServer().GetRelativeTime();
    StateTable.Start();
    ApplyBaseFrameToState(*m_data);
    UpdateTimestamps(m_data->LastTime);
    StateTable.Advance();
    if (m_data->OperatingStateEvent.IsValid()) {
        m_data->OperatingStateEvent(m_data->OperatingState);
    }
    if (m_data->OrientationLockedEvent.IsValid()) {
        m_data->OrientationLockedEvent(m_data->LockOrientation);
    }
    if (m_data->PositionLockedEvent.IsValid()) {
        m_data->PositionLockedEvent(m_data->LockPosition);
    }
}

void mts3Dconnexion::Run(void)
{
    StateTable.Start();
    if (m_data->Interface) {
        m_data->Interface->ProcessMailBoxes();
    }

    const double now = mtsManagerLocal::GetInstance()->GetTimeServer().GetRelativeTime();
    double period = now - m_data->LastTime;
    if ((period < 0.0) || (period > m_data->MaxIntegrationStep)) {
        period = m_data->MaxIntegrationStep;
    }
    m_data->LastTime = now;

    PollReports();
    IntegrateVirtualPose(period);
    if (m_data->BaseFrame.Dynamic() && !m_data->BaseFrameResolved && !m_data->BaseFrameErrorSent
        && m_data->Interface) {
        m_data->Interface->SendWarning(m_data->InterfaceName
                                       + ": failed to resolve base_frame measured_cp from "
                                       + m_data->BaseFrame.component() + "/" + m_data->BaseFrame.interface());
        m_data->BaseFrameErrorSent = true;
    }
    if (m_data->BaseFrameResolved) {
        m_data->BaseFrameErrorSent = false;
    }
    UpdateTimestamps(now);

    StateTable.Advance();
}

void mts3Dconnexion::Cleanup(void)
{
    if (m_data->Handle) {
        hid_close(m_data->Handle);
        m_data->Handle = nullptr;
    }
}

void mts3Dconnexion::UpdateTimestamps(const double & timestamp)
{
    m_data->OperatingState.SetTimestamp(timestamp);
    m_data->LocalMeasuredCs.Timestamp() = timestamp;
    m_data->MeasuredCs.Timestamp() = timestamp;
    m_data->MeasuredCp.SetTimestamp(timestamp);
    m_data->SetpointCp.SetTimestamp(timestamp);
    m_data->MeasuredCv.SetTimestamp(timestamp);
    if (m_data->Gripper.Provide) {
        m_data->GripperMeasuredJs.SetTimestamp(timestamp);
    }
}

void mts3Dconnexion::PollReports(void)
{
    if (!m_data->Handle) {
        m_data->LocalMeasuredCs.PositionIsValid() = false;
        m_data->LocalMeasuredCs.VelocityIsValid() = false;
        m_data->MeasuredCs.PositionIsValid() = false;
        m_data->MeasuredCs.VelocityIsValid() = false;
        m_data->MeasuredCp.SetValid(false);
        m_data->MeasuredCv.SetValid(false);
        if (!m_data->SentConnectionError && m_data->Interface) {
            m_data->Interface->SendError(m_data->InterfaceName + ": no HID handle available");
            m_data->SentConnectionError = true;
        }
        return;
    }

    unsigned char buffer[256];
    for (size_t reportIndex = 0; reportIndex < m_data->MaxReportsPerRun; ++reportIndex) {
        const int timeout = (reportIndex == 0)
            ? static_cast<int>(m_data->ReadTimeoutMs)
            : 0;
        const int result = hid_read_timeout(m_data->Handle, buffer, sizeof(buffer), timeout);
        if (result == 0) {
            break;
        }
        if (result < 0) {
            m_data->LocalMeasuredCs.PositionIsValid() = false;
            m_data->LocalMeasuredCs.VelocityIsValid() = false;
            m_data->MeasuredCs.PositionIsValid() = false;
            m_data->MeasuredCs.VelocityIsValid() = false;
            m_data->MeasuredCp.SetValid(false);
            m_data->MeasuredCv.SetValid(false);
            if (m_data->Interface) {
                m_data->Interface->SendError(m_data->InterfaceName + ": failed reading HID report");
            }
            hid_close(m_data->Handle);
            m_data->Handle = nullptr;
            break;
        }
        ProcessReport(buffer, result);
    }
}

void mts3Dconnexion::ProcessReport(const unsigned char * report,
                                   const int length)
{
    if (length <= 0) {
        return;
    }

    const int reportId = report[0];
    if (m_data->TranslationReport.Enabled
        && (reportId == m_data->TranslationReport.Id)) {
        ProcessMotionReport(report, length, true);
    } else if (m_data->RotationReport.Enabled
               && (reportId == m_data->RotationReport.Id)) {
        ProcessMotionReport(report, length, false);
    } else if (m_data->ButtonReport.Enabled
               && (reportId == m_data->ButtonReport.Id)) {
        ProcessButtonReport(report, length);
    }
}

void mts3Dconnexion::ProcessMotionReport(const unsigned char * report,
                                         const int length,
                                         const bool linear)
{
    const ReportConfiguration & reportConfiguration = linear
        ? m_data->TranslationReport
        : m_data->RotationReport;
    const AxisConfiguration & axisConfiguration = linear
        ? m_data->Linear
        : m_data->Angular;
    vct3 & rawValues = linear ? m_data->RawLinear : m_data->RawAngular;
    const size_t velocityOffset = linear ? 0 : 3;

    if (length < static_cast<int>(reportConfiguration.Offset + 6)) {
        return;
    }

    std::array<double, 3> raw;
    raw[0] = static_cast<double>(ReadInt16LE(report, reportConfiguration.Offset + 0));
    raw[1] = static_cast<double>(ReadInt16LE(report, reportConfiguration.Offset + 2));
    raw[2] = static_cast<double>(ReadInt16LE(report, reportConfiguration.Offset + 4));

    for (size_t target = 0; target < 3; ++target) {
        const int source = axisConfiguration.Map[target];
        const double rawValue = ((source >= 0) && (source < 3)) ? raw[source] : 0.0;
        rawValues[target] = rawValue;
        double velocity =
            axisConfiguration.Sign[target]
            * ApplyDeadbandAndScale(rawValue,
                                    axisConfiguration.FullScale,
                                    axisConfiguration.Deadband,
                                    axisConfiguration.Scale[target]);
        if ((linear && m_data->LockPosition)
            || (!linear && m_data->LockOrientation)) {
            velocity = 0.0;
        }
        m_data->LocalMeasuredCs.Velocity()[velocityOffset + target] = velocity;
    }
    m_data->LocalMeasuredCs.VelocityIsValid() = true;
    m_data->MeasuredCv.SetValid(true);
}

void mts3Dconnexion::ProcessButtonReport(const unsigned char * report,
                                         const int length)
{
    if (length <= static_cast<int>(m_data->ButtonReport.Offset)) {
        return;
    }

    uint32_t mask = 0;
    const size_t availableBytes = static_cast<size_t>(length) - m_data->ButtonReport.Offset;
    const size_t bytes = std::min<size_t>(std::min<size_t>(m_data->ButtonReport.Bytes, availableBytes), 4);
    for (size_t index = 0; index < bytes; ++index) {
        mask |= static_cast<uint32_t>(report[m_data->ButtonReport.Offset + index]) << (8 * index);
    }

    for (auto & button : m_data->Buttons) {
        const bool pressed = (button->Index < 32) && ((mask & (1u << button->Index)) != 0);
        if (button->Index >= m_data->ButtonStates.size()) {
            m_data->ButtonStates.resize(button->Index + 1, false);
        }
        m_data->ButtonStates[button->Index] = pressed;
        if (button->Pressed != pressed) {
            button->Pressed = pressed;
            if (!button->SuppressEvents) {
                prmEventButton event;
                event.SetValid(true);
                event.SetTimestamp(mtsManagerLocal::GetInstance()->GetTimeServer().GetRelativeTime());
                event.SetType(pressed ? prmEventButton::PRESSED : prmEventButton::RELEASED);
                button->Function(event);
            }
        }
    }
}

void mts3Dconnexion::IntegrateVirtualPose(const double & period)
{
    for (size_t index = 0; index < 3; ++index) {
        m_data->LocalMeasuredCs.Position().Translation()[index]
            += period * m_data->LocalMeasuredCs.Velocity()[index];
    }

    vct3 angularVelocity;
    for (size_t index = 0; index < 3; ++index) {
        angularVelocity[index] = m_data->LocalMeasuredCs.Velocity()[index + 3];
    }
    const double angularSpeed = angularVelocity.Norm();
    if (angularSpeed > 1.0e-9) {
        const vct3 axis = angularVelocity / angularSpeed;
        vctRot3 delta(vctAxAnRot3(axis, angularSpeed * period));
        m_data->LocalMeasuredCs.Position().Rotation()
            = delta * m_data->LocalMeasuredCs.Position().Rotation();
        m_data->LocalMeasuredCs.Position().Rotation().NormalizedSelf();
    }
    if (m_data->LocalMeasuredCs.VelocityIsValid()) {
        m_data->LocalMeasuredCs.PositionIsValid() = true;
        ApplyBaseFrameToState(*m_data);
    } else {
        m_data->LocalMeasuredCs.PositionIsValid() = false;
        m_data->MeasuredCs.PositionIsValid() = false;
        m_data->MeasuredCs.VelocityIsValid() = false;
        m_data->MeasuredCp.SetValid(false);
        m_data->SetpointCp.SetValid(false);
        m_data->MeasuredCv.SetValid(false);
    }

    if (m_data->Gripper.Provide) {
        double gripperVelocity = 0.0;
        if (m_data->Gripper.Emulate) {
            const bool openPressed = ButtonState(m_data->Gripper.OpenButton);
            const bool closePressed = ButtonState(m_data->Gripper.CloseButton);
            if (openPressed != closePressed) {
                gripperVelocity = (openPressed ? 1.0 : -1.0) * m_data->Gripper.Rate;
            }
            double position = m_data->GripperMeasuredJs.Position()[0] + period * gripperVelocity;
            position = std::max(m_data->Gripper.Min, std::min(m_data->Gripper.Max, position));
            m_data->GripperMeasuredJs.Position()[0] = position;
        }
        m_data->GripperMeasuredJs.Velocity()[0] = gripperVelocity;
        m_data->GripperMeasuredJs.SetValid(true);
    }
}

bool mts3Dconnexion::ButtonState(const unsigned int & index) const
{
    if (index >= m_data->ButtonStates.size()) {
        return false;
    }
    return m_data->ButtonStates[index];
}

void mts3Dconnexion::GetButtonNames(std::list<std::string> & result) const
{
    result.clear();
    for (auto & button : m_data->Buttons) {
        result.push_back(button->Name);
    }
}

void mts3Dconnexion::GetConfigurationJs(prmConfigurationJoint & configuration) const
{
    configuration = m_data->GripperConfigurationJs;
}

void mts3Dconnexion::state_command(const std::string & command)
{
    std::string humanReadableMessage;
    prmOperatingState::StateType newOperatingState;
    try {
        if (m_data->OperatingState.ValidCommand(prmOperatingState::CommandTypeFromString(command),
                                                newOperatingState,
                                                humanReadableMessage)) {
            if (command == "enable") {
                m_data->OperatingState.State() = prmOperatingState::ENABLED;
            } else if (command == "disable") {
                m_data->OperatingState.State() = prmOperatingState::DISABLED;
            } else if (command == "home") {
                m_data->OperatingState.IsHomed() = true;
                reset_pose();
            } else {
                m_data->Interface->SendStatus(m_data->InterfaceName + ": state command \""
                                              + command + "\" is not supported yet");
            }
            m_data->OperatingState.Valid() = true;
            m_data->OperatingStateEvent(m_data->OperatingState);
        } else {
            m_data->Interface->SendWarning(m_data->InterfaceName + ": " + humanReadableMessage);
        }
    } catch (std::runtime_error & e) {
        m_data->Interface->SendWarning(m_data->InterfaceName + ": " + command
                                       + " doesn't seem to be a valid state_command ("
                                       + e.what() + ")");
    }
}

void mts3Dconnexion::lock_orientation(const bool & lock)
{
    m_data->LockOrientation = lock;
    for (size_t index = 0; index < 3; ++index) {
        m_data->LocalMeasuredCs.Velocity()[index + 3] = 0.0;
    }
    ApplyBaseFrameToState(*m_data);
    if (m_data->OrientationLockedEvent.IsValid()) {
        m_data->OrientationLockedEvent(m_data->LockOrientation);
    }
}

void mts3Dconnexion::lock_position(const bool & lock)
{
    m_data->LockPosition = lock;
    for (size_t index = 0; index < 3; ++index) {
        m_data->LocalMeasuredCs.Velocity()[index] = 0.0;
    }
    ApplyBaseFrameToState(*m_data);
    if (m_data->PositionLockedEvent.IsValid()) {
        m_data->PositionLockedEvent(m_data->LockPosition);
    }
}

void mts3Dconnexion::move_cp(const prmPositionCartesianSet & position)
{
    prmBaseFrame resolvedBaseFrame;
    if (ResolveBaseFrame(*m_data, resolvedBaseFrame)) {
        vctFrm3 fixedTransform;
        fixedTransform.From(resolvedBaseFrame.transform());
        m_data->LocalMeasuredCs.Position() = fixedTransform.Inverse() * position.Goal();
    } else {
        m_data->LocalMeasuredCs.Position().Assign(position.Goal());
    }
    m_data->LocalMeasuredCs.PositionIsValid() = true;
    ApplyBaseFrameToState(*m_data);
}

void mts3Dconnexion::reset_pose(void)
{
    m_data->LocalMeasuredCs.Position().Assign(vctFrm3::Identity());
    m_data->LocalMeasuredCs.PositionIsValid() = true;
    ApplyBaseFrameToState(*m_data);
}
