# Ghost Trap - Ghostscript trapped in a sandbox

*Ghost Trap* is used to securely convert PostScript and PDF files from untrusted sources into images.
It's a modified distribution of the [GPL Ghostscript PDL interpreter](http://www.ghostscript.com/) secured and 
sandboxed using [Google Chrome sandbox technology](http://dev.chromium.org/developers/design-documents/sandbox).  The 
objective of the project is to bring best-of-breed security to Ghostscript's command-line conversion applications
on MS Windows.

For the less technical audience  *Ghost Trap* was designed with the help of 
[Peter Venkman, Egon Spengler, and Raymond Stantz](http://en.wikipedia.org/wiki/Ghostbusters). 
It can be simply explained as portable [Ecto Containment Unit](http://www.gbfans.com/equipment/ghost-trap/) which
securely holds Ghostscripts in a laser containment field :-)

[![Ghost Trap from Ghostbusters](https://raw.githubusercontent.com/PaperCutSoftware/GhostTrap/master/images/ghostbusters-ghost-trap-sized.jpg)](https://www.gbfans.com/equipment/ghost-trap/)


## Download

*Windows:* [ghost-trap-installer.exe](https://cdn1.papercut.com/files/open-source/ghost-trap/ghost-trap-installer-1.4.10.00.exe)  (version 1.4)


## Motivation

Page Description Language (PDL) interpreters are large complex native code solutions. Adobe Reader is also a PDL viewer and as evident
by the number of security updates seen over the years, maintaining a secure solution in this space is a
difficult and ongoing exercise.  

Security is one of the most important goals at [PaperCut Software](http://www.papercut.com/).  Many
PaperCut users are large organizations printing sensitive data and have high security requirements.
In 2012 we started on a project to bring one of our most requested features, the ability to view and 
archive print jobs, into our print management software. To support viewing of PostScript jobs we 
needed to reply on a PostScript interpreter.

Rather than risk PaperCut users being susceptible to zero-day attacks we decided to take a more 
proactive approach by adopting modern process-level sandboxing.  This best-practice "extra barrier"
has been made possible by mating two fantastic open source projects: Ghostscript and Google Chromium. 
*Ghost Trap* brings Chromium's best of breed sandboxing security to Ghostscript.  Although sandboxing 
is not [100% infallible](http://blog.chromium.org/2012/05/tale-of-two-pwnies-part-1.html) the increased security 
adds a very substantial barrier, and provides our users with a best-practice secure option for PostScript to image 
conversion.


## Command-Line Usage

Install *Ghost Trap* (download link above).  Here are some examples using the example PostScript files
supplied with Ghostscript.

To convert the Escher PostScript example into a PNG image *WITHOUT* sandboxing (this is just standard Ghostscript):

    C:\Users\me> "C:\Program Files (x86)\GhostTrap\bin\gsc.exe" -q -dSAFER -dNOPAUSE -dBATCH ^
                 -sDEVICE=png16m -sOutputFile=escher.png ^
                 "C:\Program Files (x86)\GhostTrap\examples\escher.ps"


To convert the same Escher example into a PNG image *WITH* sandboxing using *Ghost Trap*:

    C:\Users\me> "C:\Program Files (x86)\GhostTrap\bin\gsc-trapped.exe" -q -dNOPAUSE -dBATCH ^
                 -sDEVICE=png16m -sOutputFile=escher.png ^
                 "C:\Program Files (x86)\GhostTrap\examples\escher.ps"

To convert a multi-page PDF file into a JPEG images *WITH* sandboxing:

    C:\Users\me> "C:\Program Files (x86)\GhostTrap\bin\gsc-trapped.exe" -q -dNOPAUSE -dBATCH ^
                 -sDEVICE=jpeg "-sOutputFile=annots page %d.jpg" ^
                 "C:\Program Files (x86)\GhostTrap\examples\annots.pdf"

`gsc-trapped.exe` is the sandboxed version of `gsc.exe`.  It should behave the same
as the standard Ghostscript console command as [documented](https://ghostscript.readthedocs.io/en/gs10.0.0/Use.html),
with the following known exceptions:

 *  The input and output files must be on a local disk (no network shares).
 *  The `-dSAFER` mode is always enabled by default.
 *  Defining custom/extra FONT or LIB paths on the command line is not allowed.


## How it works

`gsc-trapped.exe` first determines a whitelist of resources required to perform the conversion.  It then 
execs a child process within a strongly contained sandbox to perform the task. The whitelist of allowed resources 
is dynamically constructed by determining the input file and output file/directory from the supplied 
command-line arguments. The Ghostscript interpreter's access rights is restricted and it may only access:

 *  Read only access to the Windows Fonts directory.
 *  Read only access to application-level registry keys (HKLM\Software\GPL Ghostscript).
 *  Read only access to Ghostscript's lib folder resources.
 *  Read only access to the input file.
 *  Write access to the user-level Temp directory.
 *  Write access to the output directory (OutputFile).

The sandbox also constrains the execution process on an isolated desktop session to prevent 
[shatter attacks](http://en.wikipedia.org/wiki/Shatter_attack) and limits IPC and other potential
escape vectors.


## Release History

### [1.4.10.00] - 2023-01-06
 * Updated to Ghostscript 10.00.0.20220921.
 * Updated to the latest Chromium Sandbox [as of 2022-12-15](https://chromium.googlesource.com/chromium/src/+/1a554a4863f66c922398e91691212a54a8f11ea0)).
 * Fixed the sandbox tests to no longer report a missing output file.
 * Fixed the sandbox failure tests to run when requested.

### [1.3.9.27] - 2019-06-14
 *  __Breaking change: installer now includes 64-bit binaries only.__
 *  Updated to Ghostscript 9.27.
 *  Updated to the latest Chromium Sandbox (as of [2019-04-14](https://chromium.googlesource.com/chromium/src/+/2d57e5b8afc6d01b344a8d95d3470d46b35845c5)).
 *  The build script now builds 64-bit binaries (only). The architecture is no longer part of the executable names. I.e.
    `win32` and `win64` are dropped from the names.
 *  For backwards compatibility (path references to binaries), the installer script copies the 64-bit binaries to their
    old 32-bit named equivalents. E.g. `gswin32c-trapped.exe` is a copy of `gsc-trapped.exe`.

### [1.2.9.10] - 2013-10-11
 *  Updated to Ghostscript 9.10.

### [1.2.9.07] - 2013-03-04
 *  Addressed an issue that would result in Ghost Trap returning the wrong exit code.

### [1.1.9.07] - 2013-03-01
 *  Updated to Ghostscript 9.07.
 *  `-dSAFER` is now an enforced default.
 *  Updated license to Affero GPL.
 *  Minor code cleanup to remove some FIXME's.

### [1.0.9.06] - 2013-01-14 
 *  Initial public release.


## Future

The following future refinements are planned:

 *  Sandbox other executables in the GhostPDL project (e.g `pcl6.exe`).
 *  Support custom FONT and LIB paths defined on the command line (read only access).
 *  Look at sandbox options on Linux.


## Authors

![PaperCut Software Logo](http://www.papercut.com/images/logo_papercut.png)

*Ghost Trap* is open source software developed by Chris Dance with the support of 
[PaperCut Software](http://www.papercut.com/).


### Requirements

 *  Visual Studio 2019 or 2017
 *  [Inno Setup 6](http://www.jrsoftware.org/isinfo.php)


## Building

 1. Check out the [Ghost Trap Source Code](https://github.com/PaperCutSoftware/GhostTrap).

 2. Follow the [Chromium source checkout and build instructions](https://chromium.googlesource.com/chromium/src/+/master/docs/windows_build_instructions.md).

     *  When creating the `chromium` directory for source checkout, create it at `GhostTrap/third-party/chromium`.
     *  Take note of the `--no-history` flag to `fetch`, which will significantly speed up the checkout.
     *  Ensure that `gn gen out\Default` runs successfully.
     *  The Visual Studio setup steps are not necessary.

 3. Download the [GhostPDL source](https://www.ghostscript.com/download/gpdldnld.html) to
    `GhostTrap/third-party/ghostpdl`.

 4. Compile 64-bit binaries for Ghostscript, GhostPCL and GhostXPS. At the time of writing, this involved:

     *  Opening `ghostpdl/windows/GhostPDL.sln` in Visual Studio to trigger a project structure upgrade.
     *  Running `msbuild windows/GhostPDL.sln /p:Configuration=Release /p:Platform=x64` from the Developer Command
        Prompt.

 5. Run the `GhostTrap/build.bat` build script.

    The installer will be output to `GhostTrap/target/ghost-trap-installer-${version}.exe`.


## License

*Ghost Trap* is open source software licensed under the Affero GPL:

    Copyright (c) 2012-2023 PaperCut Software Pty Ltd http://www.papercut.com/

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.


[1.4.10.00]: https://github.com/PaperCutSoftware/GhostTrap/compare/v1.3.9.27...v1.4.10.00
[1.3.9.27]: https://github.com/PaperCutSoftware/GhostTrap/compare/v1.2.9.10...v1.3.9.27
[1.2.9.10]: https://github.com/PaperCutSoftware/GhostTrap/compare/v1.2.9.07...v1.2.9.10
[1.2.9.07]: https://github.com/PaperCutSoftware/GhostTrap/compare/v1.1.9.07...v1.2.9.07
[1.1.9.07]: https://github.com/PaperCutSoftware/GhostTrap/compare/v1.0.9.06...v1.1.9.07
[1.0.9.06]: https://github.com/PaperCutSoftware/GhostTrap/releases/tag/v1.0.9.06
