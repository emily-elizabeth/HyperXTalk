all: \
    $(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.extension-utils/extension-utils.livecodescript \
    $(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.oauth2/oauth2.livecodescript \
    $(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.getopt/getopt.livecodescript \
    $(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.mime/mime.livecodescript \
    $(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.drawing/drawing.livecodescript \
    $(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.dropbox/dropbox.livecodescript \
    $(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.diff/diff.livecodescript \
    $(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.messageauthentication/messageauthentication.livecodescript \
    $(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.httpd/httpd.livecodescript \
    $(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.qr/qr.livecodescript

$(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.extension-utils/extension-utils.livecodescript \
    : \
    script-libraries/extension-utils/extension-utils.livecodescript
	@mkdir -p "$(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.extension-utils"
	@echo note: "Building script extension extension-utils.livecodescript"
	"$(BUILT_PRODUCTS_DIR)/server-community" script-libraries/extension-utils/resources/extension-utils.lc "$(not_a_real_variable)buildextension" dummy1 dummy2 dummy3 ../ide-support/revdocsparser.livecodescript "script-libraries/extension-utils" "extension-utils.livecodescript" "$(BUILT_PRODUCTS_DIR)/packaged_extensions" "$(not_a_real_variable)false"

$(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.oauth2/oauth2.livecodescript \
    : \
    script-libraries/oauth2/oauth2.livecodescript
	@mkdir -p "$(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.oauth2"
	@echo note: "Building script extension oauth2.livecodescript"
	"$(BUILT_PRODUCTS_DIR)/server-community" script-libraries/extension-utils/resources/extension-utils.lc "$(not_a_real_variable)buildextension" dummy1 dummy2 dummy3 ../ide-support/revdocsparser.livecodescript "script-libraries/oauth2" "oauth2.livecodescript" "$(BUILT_PRODUCTS_DIR)/packaged_extensions" "$(not_a_real_variable)false"

$(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.getopt/getopt.livecodescript \
    : \
    script-libraries/getopt/getopt.livecodescript
	@mkdir -p "$(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.getopt"
	@echo note: "Building script extension getopt.livecodescript"
	"$(BUILT_PRODUCTS_DIR)/server-community" script-libraries/extension-utils/resources/extension-utils.lc "$(not_a_real_variable)buildextension" dummy1 dummy2 dummy3 ../ide-support/revdocsparser.livecodescript "script-libraries/getopt" "getopt.livecodescript" "$(BUILT_PRODUCTS_DIR)/packaged_extensions" "$(not_a_real_variable)false"

$(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.mime/mime.livecodescript \
    : \
    script-libraries/mime/mime.livecodescript
	@mkdir -p "$(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.mime"
	@echo note: "Building script extension mime.livecodescript"
	"$(BUILT_PRODUCTS_DIR)/server-community" script-libraries/extension-utils/resources/extension-utils.lc "$(not_a_real_variable)buildextension" dummy1 dummy2 dummy3 ../ide-support/revdocsparser.livecodescript "script-libraries/mime" "mime.livecodescript" "$(BUILT_PRODUCTS_DIR)/packaged_extensions" "$(not_a_real_variable)false"

$(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.drawing/drawing.livecodescript \
    : \
    script-libraries/drawing/drawing.livecodescript
	@mkdir -p "$(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.drawing"
	@echo note: "Building script extension drawing.livecodescript"
	"$(BUILT_PRODUCTS_DIR)/server-community" script-libraries/extension-utils/resources/extension-utils.lc "$(not_a_real_variable)buildextension" dummy1 dummy2 dummy3 ../ide-support/revdocsparser.livecodescript "script-libraries/drawing" "drawing.livecodescript" "$(BUILT_PRODUCTS_DIR)/packaged_extensions" "$(not_a_real_variable)false"

$(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.dropbox/dropbox.livecodescript \
    : \
    script-libraries/dropbox/dropbox.livecodescript
	@mkdir -p "$(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.dropbox"
	@echo note: "Building script extension dropbox.livecodescript"
	"$(BUILT_PRODUCTS_DIR)/server-community" script-libraries/extension-utils/resources/extension-utils.lc "$(not_a_real_variable)buildextension" dummy1 dummy2 dummy3 ../ide-support/revdocsparser.livecodescript "script-libraries/dropbox" "dropbox.livecodescript" "$(BUILT_PRODUCTS_DIR)/packaged_extensions" "$(not_a_real_variable)false"

$(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.diff/diff.livecodescript \
    : \
    script-libraries/diff/diff.livecodescript
	@mkdir -p "$(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.diff"
	@echo note: "Building script extension diff.livecodescript"
	"$(BUILT_PRODUCTS_DIR)/server-community" script-libraries/extension-utils/resources/extension-utils.lc "$(not_a_real_variable)buildextension" dummy1 dummy2 dummy3 ../ide-support/revdocsparser.livecodescript "script-libraries/diff" "diff.livecodescript" "$(BUILT_PRODUCTS_DIR)/packaged_extensions" "$(not_a_real_variable)false"

$(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.messageauthentication/messageauthentication.livecodescript \
    : \
    script-libraries/messageauthentication/messageauthentication.livecodescript
	@mkdir -p "$(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.messageauthentication"
	@echo note: "Building script extension messageauthentication.livecodescript"
	"$(BUILT_PRODUCTS_DIR)/server-community" script-libraries/extension-utils/resources/extension-utils.lc "$(not_a_real_variable)buildextension" dummy1 dummy2 dummy3 ../ide-support/revdocsparser.livecodescript "script-libraries/messageauthentication" "messageauthentication.livecodescript" "$(BUILT_PRODUCTS_DIR)/packaged_extensions" "$(not_a_real_variable)false"

$(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.httpd/httpd.livecodescript \
    : \
    script-libraries/httpd/httpd.livecodescript
	@mkdir -p "$(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.httpd"
	@echo note: "Building script extension httpd.livecodescript"
	"$(BUILT_PRODUCTS_DIR)/server-community" script-libraries/extension-utils/resources/extension-utils.lc "$(not_a_real_variable)buildextension" dummy1 dummy2 dummy3 ../ide-support/revdocsparser.livecodescript "script-libraries/httpd" "httpd.livecodescript" "$(BUILT_PRODUCTS_DIR)/packaged_extensions" "$(not_a_real_variable)false"

$(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.qr/qr.livecodescript \
    : \
    script-libraries/qr/qr.livecodescript
	@mkdir -p "$(BUILT_PRODUCTS_DIR)/packaged_extensions/com.livecode.library.qr"
	@echo note: "Building script extension qr.livecodescript"
	"$(BUILT_PRODUCTS_DIR)/server-community" script-libraries/extension-utils/resources/extension-utils.lc "$(not_a_real_variable)buildextension" dummy1 dummy2 dummy3 ../ide-support/revdocsparser.livecodescript "script-libraries/qr" "qr.livecodescript" "$(BUILT_PRODUCTS_DIR)/packaged_extensions" "$(not_a_real_variable)false"
