GestLib: A C++ library for multitouch gesture recognition on Linux.  

Library designed to interpret raw multitouch events from the Linux input system (/dev/input/event*) 
and map them to high-level gesture events.  

Recognizes the following gestures:  
- Tap  
- Hold  
- Drag - sustained movement after Hold.  
- Swipe - directional finger movement.  
- Double Tap  
- Double Tap Swipe - directional finger movement after Double Tap  

Provides a C++ interface for integration into your projects.  
Project is in alpha. Core gesture parsing logic is functional. 
Known bugs exist. See TODO.  

Contributions and feedback are welcome.  

Acknowledgements  

Parts of the architecture (e.g. gesture state management via state machines, input filtering in TouchDriver.cpp) were iteratively designed with assistance from Google's Gemini LLM. Prompts, code generations, and evaluations were directed by the author. Final logic and implementation decisions remain human-authored.  

Requirements:  
- libevdev  

Known issues:  
- Sometimes incomplete termination events for complex gestures (e.g., Drag, Swipe).  
- Double Tap fails with high-speed taps due to timing threshold limits.  

TODO:  
[ ] - Double Tap Circular Gesture  
[ ] - Fake Touch Driver thru glfw mouse and keyboard  
[ ] - 2 and 3 finger gestures recognition.  
[ ] - Bug fixes  
[ ] - Implement some test cases  
[ ] - Synchronization of SYN_DROPPED(for now just ignored)  
[ ] - Code comments  
[ ] - Code cleanup  

Tested on Raspberry Pi 4 with generic USB touchscreen devices.  


log:  
08.08.25  
Bugfix: GestureState for complex gestures always was Start.  
Now state working well.

04.08.25  
Added enum GestureState, now instead of using separate structures for complex Gestures
there is only one structure with state inside(see example) -> less callbacks  
  
Removed enum GestureType
