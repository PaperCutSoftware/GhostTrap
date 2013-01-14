Ghost Trap 3rd Party Dependencies

* Google Chromium (sandbox sub project)
* GhostPDL - Ghostscript PDL project

Instructions:

1) Place the source code for Ghost Trap's 3rd party dependencies in this 
   directory. After this you should be able to find the following directories:
   
   [ghost-trap]/third-party/chromium/src/sandbox
   [ghost-trap]/third-party/ghostpdl/win32

   Note: You may need to remove the version number from the top level 
   directories.

2) Perform a 32bit Release compile on each dependency. Take care following
   the build instructions for Google Chromium. It's very involved! You will not
   need to compile the whole Chromium source - just the "sandbox" sub project
   will be enough to generate the required dependencies.

