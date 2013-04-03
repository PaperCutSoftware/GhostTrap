Ghost Trap - Ghostscript trapped in a sandbox
======

*Ghost Trap* is used to securely convert PostScript and PDF files from untrusted sources into images.
It's a modified distribution of the [GPL Ghostscript PDL interpreter](http://www.ghostscript.com/) secured and 
sandboxed using [Google Chrome sandbox technology](http://dev.chromium.org/developers/design-documents/sandbox).  The 
objective of the project is to bring best-of-breed security to Ghostscript's command-line conversion applications
on MS Windows.

For the less technical audience  *Ghost Trap* was designed with the help of 
[Peter Venkman, Egon Spengler, and Raymond Stantz](http://en.wikipedia.org/wiki/Ghostbusters). 
It can be simply explained as portable [Ecto Containment Unit](http://www.gbfans.com/equipment/ghost-trap/) which
securely holds Ghostscripts in a laser containment field :-)

<a href="http://www.gbfans.com/equipment/ghost-trap/" 
    title="Ghostbusters Fan -  love the '80's! (used as a parody)">
    <img src="https://github.com/codedance/GhostTrap/raw/master/images/ghostbusters-ghost-trap-sized.jpg">
</a>


##Download

*Windows:* [ghost-trap-installer.exe](http://cdn.papercut.com/anonftp/pub/open-source/ghost-trap/ghost-trap-installer-1.2.9.07.exe)  (version 1.2)

##Motivation
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


##Command-Line Usage

Install *Ghost Trap* (download link above).  Here are some examples using the example PostScript files
supplied with Ghostscript.

To convert the Escher PostScript example into a PNG image *WITHOUT* sandboxing (this is just standard Ghostscript):

    C:\Users\me> "C:\Program Files (x86)\GhostTrap\bin\gswin32c.exe" -q -dSAFER -dNOPAUSE -dBATCH ^
                 -sDEVICE=png16m -sOutputFile=escher.png ^
                 "C:\Program Files (x86)\GhostTrap\examples\escher.ps"


To convert the same Escher example into a PNG image *WITH* sandboxing using *Ghost Trap*:

    C:\Users\me> "C:\Program Files (x86)\GhostTrap\bin\gswin32c-trapped.exe" -q -dNOPAUSE -dBATCH ^
                 -sDEVICE=png16m -sOutputFile=escher.png ^
                 "C:\Program Files (x86)\GhostTrap\examples\escher.ps"

To convert a multi-page PDF file into a JPEG images *WITH* sandboxing:

    C:\Users\me> "C:\Program Files (x86)\GhostTrap\bin\gswin32c-trapped.exe" -q -dNOPAUSE -dBATCH ^
                 -sDEVICE=jpeg "-sOutputFile=annots page %d.jpg" ^
                 "C:\Program Files (x86)\GhostTrap\examples\annots.pdf"

```gswin32c-trapped.exe``` is the sandboxed version of ``gswin32c.exe``.  It should behave the same
as the standard Ghostscript console command as [documented](http://ghostscript.com/doc/9.07/Use.htm),
with the following known exceptions:

 * The input and output files must be on a local disk (no network shares).
 * The ```-dSAFER``` mode is always enabled by default.
 * Defining custom/extra FONT or LIB paths on the command line is not allowed.


##How it works

```gswin32c-trapped.exe``` first determines a whitelist of resources required to perform the conversion.  It then 
execs a child process within a strongly contained sandbox to perform the task. The whitelist of allowed resources 
is dynamically constructed by determining the input file and output file/directory from the supplied 
command-line arguments. The Ghostscript interpreter's access rights is restricted and it may only access:

 * Read only access to the Windows Fonts directory.
 * Read only access to application-level registry keys (HKLM\Software\GPL Ghostscript).
 * Read only access to Ghostscript's lib folder resources.
 * Read only access to the input file.
 * Write access to the user-level Temp directory.
 * Write access to the output directory (OutputFile).

The sandbox also constrains the execution process on an isolated desktop session to prevent 
[shatter attacks](http://en.wikipedia.org/wiki/Shatter_attack") and limits IPC and other potential
escape vectors.

##Release History

**2013-03-04**
 * Addressed an issue that would result in Ghost Trap returning the wrong exit code.
 * Rolled to version 1.2

**2013-03-01**
 * Compiled against Ghostscript 9.07.
 * ```-dSAFER``` is now and enforced default.
 * Updated license to AFFERO GPL.
 * Minor code cleanup to remove some FIXME's.
 * Rolled to version 1.1

**2013-01-14** 
 * Initial public release.


##Future

The following future refinements are planned:

 * Sandbox other executable in the GhostPDL project (e.g ```pcl6.exe```).
 * Support custom FONT and LIB paths defined on the command line (read only access).
 * Look at sandbox options on Linux.
 * 64bit version when/if the Chromium sandbox supports it.


##Authors

![PaperCut Software Logo](http://www.papercut.com/images/logo_papercut.png)

*Ghost Trap* is open source software developed by Chris Dance with the support of 
[PaperCut Software](http://www.papercut.com/).


##Developers

To build Ghost Trap from source, here is a brief flow:

 1. Clone this git repo.

 2. Download Google Chromium source into the third-party directory as documented in ```[ghost-trap]/third-party/README.txt```

 3. Download GhostPDL source into the third-party directory as documented in ```[ghost-trap]/third-party/README.txt```

 4. Perform a ```32bit Release``` compile on each dependency (follow the project's documentation). 
    Note: Building Chromium is very involved! Follow the directions carefully. You will not need to compile whole 
    Chromium source. Just the "sandbox" sub project will be enough to generate the required dependencies.

 5. Install [INNO setup](http://www.jrsoftware.org/isinfo.php).

 6. Run ```build.bat```


##License

*Ghost Trap* is open source software licensed under the Affero GPL:

    Copyright (c) 2012-2013 PaperCut Software Int. Pty. Ltd. http://www.papercut.com/

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
   
