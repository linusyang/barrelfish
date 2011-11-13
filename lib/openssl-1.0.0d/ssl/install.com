$! INSTALL.COM -- Installs the files in a given directory tree
$!
$! Author: Richard Levitte <richard@levitte.org>
$! Time of creation: 22-MAY-1998 10:13
$!
$! P1	root of the directory tree
$!
$	IF P1 .EQS. ""
$	THEN
$	    WRITE SYS$OUTPUT "First argument missing."
$	    WRITE SYS$OUTPUT -
		  "It should be the directory where you want things installed."
$	    EXIT
$	ENDIF
$
$	IF (F$GETSYI("CPU").LT.128)
$	THEN
$	    ARCH := VAX
$	ELSE
$	    ARCH = F$EDIT( F$GETSYI( "ARCH_NAME"), "UPCASE")
$	    IF (ARCH .EQS. "") THEN ARCH = "UNK"
$	ENDIF
$
$	ROOT = F$PARSE(P1,"[]A.;0",,,"SYNTAX_ONLY,NO_CONCEAL") - "A.;0"
$	ROOT_DEV = F$PARSE(ROOT,,,"DEVICE","SYNTAX_ONLY")
$	ROOT_DIR = F$PARSE(ROOT,,,"DIRECTORY","SYNTAX_ONLY") -
		   - "[000000." - "][" - "[" - "]"
$	ROOT = ROOT_DEV + "[" + ROOT_DIR
$
$	DEFINE/NOLOG WRK_SSLROOT 'ROOT'.] /TRANS=CONC
$	DEFINE/NOLOG WRK_SSLXLIB WRK_SSLROOT:['ARCH'_LIB]
$	DEFINE/NOLOG WRK_SSLINCLUDE WRK_SSLROOT:[INCLUDE]
$	DEFINE/NOLOG WRK_SSLXEXE WRK_SSLROOT:['ARCH'_EXE]
$
$	IF F$PARSE("WRK_SSLROOT:[000000]") .EQS. "" THEN -
	   CREATE/DIR/LOG WRK_SSLROOT:[000000]
$	IF F$PARSE("WRK_SSLXLIB:") .EQS. "" THEN -
	   CREATE/DIR/LOG WRK_SSLXLIB:
$	IF F$PARSE("WRK_SSLINCLUDE:") .EQS. "" THEN -
	   CREATE/DIR/LOG WRK_SSLINCLUDE:
$	IF F$PARSE("WRK_SSLXEXE:") .EQS. "" THEN -
	   CREATE/DIR/LOG WRK_SSLXEXE:
$
$	EXHEADER := ssl.h,ssl2.h,ssl3.h,ssl23.h,tls1.h,dtls1.h,kssl.h
$	E_EXE := ssl_task
$	LIBS := LIBSSL,LIBSSL32
$
$	XEXE_DIR := [-.'ARCH'.EXE.SSL]
$
$	COPY 'EXHEADER' WRK_SSLINCLUDE:/LOG
$	SET FILE/PROT=WORLD:RE WRK_SSLINCLUDE:'EXHEADER'
$
$	I = 0
$ LOOP_EXE: 
$	E = F$EDIT(F$ELEMENT(I, ",", E_EXE),"TRIM")
$	I = I + 1
$	IF E .EQS. "," THEN GOTO LOOP_EXE_END
$	SET NOON
$	IF F$SEARCH(XEXE_DIR+E+".EXE") .NES. ""
$	THEN
$	  COPY 'XEXE_DIR''E'.EXE WRK_SSLXEXE:'E'.EXE/log
$	  SET FILE/PROT=W:RE WRK_SSLXEXE:'E'.EXE
$	ENDIF
$	SET ON
$	GOTO LOOP_EXE
$ LOOP_EXE_END:
$
$	I = 0
$ LOOP_LIB: 
$	E = F$EDIT(F$ELEMENT(I, ",", LIBS),"TRIM")
$	I = I + 1
$	IF E .EQS. "," THEN GOTO LOOP_LIB_END
$	SET NOON
$! Object library.
$	IF F$SEARCH(XEXE_DIR+E+".OLB") .NES. ""
$	THEN
$	  COPY 'XEXE_DIR''E'.OLB WRK_SSLXLIB:'E'.OLB/log
$	  SET FILE/PROT=W:RE WRK_SSLXLIB:'E'.OLB
$	ENDIF
$! Shareable image.
$	IF F$SEARCH(XEXE_DIR+E+".EXE") .NES. ""
$	THEN
$	  COPY 'XEXE_DIR''E'.EXE WRK_SSLXLIB:'E'.EXE/log
$	  SET FILE/PROT=W:RE WRK_SSLXLIB:'E'.EXE
$	ENDIF
$	SET ON
$	GOTO LOOP_LIB
$ LOOP_LIB_END:
$
$	EXIT
