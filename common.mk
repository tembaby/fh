#### $Id: common.mk,v 1.10 2003/06/05 14:03:55 te Exp $ ####
# Tamer Embaby <tsemba@menanet.net> 
#
# Generic common makefile for C projects (binaries/static/shared).
#

lex		: $(LEX_FILES)
	lex $(LEX_FLAGS) $(LEX_FILES)

yacc		: $(YACC_FILES)
	yacc $(YACC_FLAGS) $(YACC_FILES)

install		: all
	/bin/cp -p $(TARGET) $(INSTALLDIR)

sync		:
	@wsync $(WSYNC_PARAM)

force		: clean all

tar		: distrib_init
	@if [ -e $(TARBALL) ] ; then \
		echo "Cleaning up ..." ; \
		rm -f $(TARBALL) ; \
	fi ; \
	cd .. ; \
	if [ -e "$(PROGDIR)/.wsync.conf" ] ; then \
		mkdir -p /tmp/$(PROGDIR) >/dev/null 2>&1 ; \
		mv -i $(PROGDIR)/.wsync.conf /tmp/$(PROGDIR) ; \
		if [ $$? -eq 0 ] ; then \
			echo "==> \`\`$(PROGDIR)'' secured." ; \
		else \
			echo "SECURITY FAILED!" ; \
			exit 1 ; \
		fi ; \
	fi ; \
	echo -n "Creating tar ball ... " ; \
	tar $(TAR_ARGS) $(TARBALL) $(PROGDIR) ; \
	test $$? -eq 0 && echo "done." ; \
	mv $(TARBALL) $(PROGDIR) ; \
	if [ -e "/tmp/$(PROGDIR)/.wsync.conf" ] ; then \
		mv -i /tmp/$(PROGDIR)/.wsync.conf $(PROGDIR) ; \
		rm -rf /tmp/$(PROGDIR) ; \
	fi
.if defined(DISTRIB_EXTERNAL_FILES)
.	for ext_file in ${DISTRIB_EXTERNAL_FILES}
		@rm ${ext_file}
.	endfor
.endif

distrib_init	:
.if defined(DISTRIB_EXTERNAL_FILES)
.	for __ext_file in ${DISTRIB_EXTERNAL_FILES}
		@cp $(BASEDIR)/${__ext_file} .
.	endfor
.endif

upload		:
	@$(BASEDIR)/upload.sh $(TARBALL) $(PROGDIR)

.if defined(SUB_DIRS)
sub_dirs_build	:
.	for __sub_dir in ${SUB_DIRS}
		@echo "==> Building dependency \`\`${__sub_dir}'' ..."
		@cd  ${__sub_dir} ; \
		make -f Makefile
		@echo "==> Build done for \`\`${__sub_dir}''."
		@echo ""
.	endfor
.else
sub_dirs_build	:
.endif

clean		:
	@rm -rf *.o [Ee]rrs out output $(TARGET) $(TARGET).core $(CLEAN_EXTRA)
.if defined(SUB_DIRS)
.	for __sub_dir in ${SUB_DIRS}
		@echo "==> Cleaning up for \`\`${__sub_dir}'' ..."
		@cd  ${__sub_dir} ; \
		make clean -f Makefile
.	endfor
.endif

import		: import_creat_dirs
.if defined(EXT_FILES)
.	for __ext_file in ${EXT_FILES}
		@echo "==> $(IMPORT_CMD) $(IMPORT_ARGS) \
			${EXT_ROOT}/${__ext_file} ${__ext_file}"
		@$(IMPORT_CMD) $(IMPORT_ARGS) \
			${EXT_ROOT}/${__ext_file} ${__ext_file}
.	endfor

# Same thing but with path name stripped from target file.
.if defined(EXT_FILES_NP)
.	for __ext_file in ${EXT_FILES_NP}
		@echo "==> $(IMPORT_CMD) $(IMPORT_ARGS) \
			${EXT_ROOT}/${__ext_file} ${__ext_file:T}"
		@$(IMPORT_CMD) $(IMPORT_ARGS) \
			${EXT_ROOT}/${__ext_file} ${__ext_file:T}
.	endfor
.endif
.endif

import_creat_dirs:
	@echo "==> Initiali[zs]ing extra directories if necessary ..."
	if [ ! -z "${EXT_DIRS}" ] ; then \
		(mkdir $(EXT_DIRS) >/dev/null 2>&1 || exit 0) ; \
	else \
		exit 0 ; \
	fi

changelog	:
	$(CL_BIN) $(CL_ARGS)

