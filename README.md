# Call of Duty 5: World at War Demo Rewinding
This is a proof-of-concept to show how one can rewind Call of Duty 5: World at War (Steam / retail) demos. Unlike some later Call of Duty's, CoD5 does not have 'native' support for demo rewinding.
## How to use
Import the two .cpp files and the header file in a new dll project, compile as x86, and inject the compiled dll into Call of Duty 5 before playing back a demo. The default buttons for interaction are as follows:
- Right control button for rewinding one time (1000 ms by default). To rewind more you can simply keep right ctrl pressed down;
- Right alt button to rewind all the way back to the beginning;
- Right shift button to eject the dll. It's not recommended to use this while the demo is playing.
## Tested on
This was tested on Call of Duty 5 v1.7, Steam and retail; the Plutonium client was not part of the testing process.
## Bugs
There's a myriad of glitches/bugs that occur infrequently; this seems to exacerbate when you keep rewinding. For the majority of the time it works as intended, though.
## Miscellaneous
Commercial use and any (implicit) financial gain is prohibited. Credits would be appreciated if you use this code, or parts of it.
