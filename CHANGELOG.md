# Change Log
All notable changes to this project will be documented in this file.
# Revision 1.1
- improved error handling
- improved logging module
- C module ffmeg-decode converted to C++ class FFMpegDecode
- delegate interface changed to *_onAction format e.g. portal_onDevicePacketReceive
code style changes for consistency (indentation, trailing space removal, etc.)
Boolean data members and functions changed to m_isAction format e.g. Thread::isStopped()
const-correctness
- prefix m_* for data members
- uniform brace initialization for variables and data members (wherever applicable)
- dead and commented code cleanup
- normalization of deeply nested constructs
- replacement of new with std::make_* utility functions wherever applicable
- addition of missing header #if* guards
- final specifier to restrict inheritance
- override for overridden virtual methods
- change of .h/.c with .hpp/.cpp
