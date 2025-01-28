#include "wled.h"
/*
 * Register your v2 usermods here!
 *   (for v1 usermods using just usermod.cpp, you can ignore this file)
 */

/*
 * Add/uncomment your usermod filename here (and once more below)
 * || || ||
 * \/ \/ \/
 */
//#include "../usermods/EXAMPLE_v2/usermod_v2_example.h"


#ifdef USERMOD_AUDIOREACTIVE
  #include "../usermods/audioreactive/audio_reactive.h"
#endif


#ifdef USERMOD_TTGO_TDISPLAY_OUTPUT
#include "../usermods/usermod_v2_TTGO-TDisplay_output/usermod.h"
#endif

void registerUsermods()
{
/*
   * Add your usermod class name here
   * || || ||
   * \/ \/ \/
   */
  //UsermodManager::add(new MyExampleUsermod());


  #ifdef USERMOD_AUDIOREACTIVE
  UsermodManager::add(new AudioReactive());
  #endif
 

  #ifdef USERMOD_TTGO_TDISPLAY_OUTPUT
  usermods.add(new TTGOTDisplayOutputUsermod());
  #endif
}
