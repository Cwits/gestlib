GestLib: A C++ userspace library for multitouch gesture recognition on Linux.  

Library designed to interpret raw multitouch events from the Linux input system (/dev/input/event*) 
and map them to high-level gesture events.  

Recognizes the following gestures:  
- Tap  
- Hold - can be moved
- Drag - sustained movement after Hold.  
- Swipe - directional finger movement.  
- Double Tap  
- Double Tap Swipe - directional finger movement after Double Tap  
- Zoom
- Two Finger Tap
- Two Finger Swipe

Provides a C++ interface for integration into your projects.  
Project is in alpha. Core gesture parsing logic is functional. 
Known bugs exist. See TODO.  

Requirements:  
- libevdev  
- libsdl2 (for examples and fakeSDL driver)
- libsdl2-ttf-dev (for examples only)

Known issues:  
- Sometimes incomplete termination events for complex gestures (e.g., Drag, Hold).
- Zoom and Two Finger Swipe might be mixed up(e.g. when expecting Zoom - Two Finger Swipe might apear) due to similar nature of gestures.   
- Double Tap fails with high-speed taps due to timing threshold limits.  

Contributions and feedback are welcome.  

Acknowledgements  

Parts of the architecture (e.g. gesture state management via state machines, input filtering in TouchDriver.cpp) were iteratively designed with assistance from Google's Gemini LLM. Prompts, code generations, and evaluations were directed by the author. Final logic and implementation decisions remain human-authored.  

TODO:  
[ ] - Fake Touch Driver  
[\] - 2 and 3 finger gestures recognition.
[ ] - Synchronization of SYN_DROPPED(for now just ignored),

Tested on Raspberry Pi 4 with generic USB touchscreen devices.  