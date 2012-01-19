#include <saw3Dconnexion/osa3Dconnexion.h>
#include <iostream>
#include <iomanip>

int main(){

  osa3Dconnexion spacenavigator;
  spacenavigator.Initialize();

  while( 1 ){

    osa3Dconnexion::Event event = spacenavigator.WaitForEvent();
    switch( event.type ){

    case osa3Dconnexion::Event::MOTION:
      std::cout << std::setw(10) << event.data[0]
		<< std::setw(10) << event.data[1]
		<< std::setw(10) << event.data[2]
		<< std::setw(10) << event.data[3]
		<< std::setw(10) << event.data[4]
		<< std::setw(10) << event.data[5] << std::endl;
      break;

    case osa3Dconnexion::Event::BUTTON_PRESSED:
      std::cout << "Button " << event.button << " pressed." << std::endl;
      break;

    case osa3Dconnexion::Event::BUTTON_RELEASED:
      std::cout << "Button " << event.button << " released." << std::endl;
      break;

    }
    
  }

  return 0;
}
