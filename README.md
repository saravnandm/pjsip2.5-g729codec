# pjsip2.5-g729codec

Patches to integrate g729 codec with pjsip-2.5 library.

Steps:  
+ Download PJSip-2.5 [here](http://www.pjsip.org/download.htm)  
+ Unzip the downloaded file and edit the permission of enter folder(PJSip-2.5) to 777     
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;$ chmod -R 777 /pjproject-2.5  
+ Apply pjproject-2.5.diff patch first.  
+ Copy the source and build files of g729 codec from "./pjsip2.5-g729codec/pjproject-2.5/" to the corresponding location of your pjproject-2.5. Make sure you are moving the files to the same location as patch(pjproject-2.5). Create the dir structure if not exists.

Note:
** Tested with pjsip2.5 only  
** g729 codec is not a open source library. So make sure you have licence to use.  
** g729 codec library(pj.diff file) took from linphone opensource project.  


COMMON ERRORS :  
 
Error:   
bash: ./configure: /bin/sh^M: bad interpreter: No such file or directory  

Fix:  
open your script with vi or vim and enter in vi command mode (key ESC), then type this:

:set fileformat=unix  
Press return and finally save it  
:x! or :wq!

Error:  
Undefined symbols for architecture armv7s:  
  "_main", referenced from:  
     implicit entry/start for main executable  
ld: symbol(s) not found for architecture armv7s  
clang: error: linker command failed with exit code 1 (use -v to see invocation)  
make[3]: *** [../bin/samples/arm-apple-darwin9/pjsua2_demo] Error 1  
make[2]: *** [pjsua2_demo] Error 2  
make[1]: *** [samples] Error 2  
make: *** [all] Error 1  

Fix:  
$ make clean  
$ rm pjsip-apps/build/output/pjsua2_sample-arm-apple-darwin9/pjsua2_demo.o

