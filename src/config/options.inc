/* Option definitions. */
/* $Id: options.inc,v 1.24 2003/11/04 17:11:57 zas Exp $ */

/* TODO: Autogenerate this file from something more human readable. --jonas */
/* TODO: Make #ifdef FEATURE more consistent. */

/* A few guidelines for this file: */
/* 1. Keep options in alphabetical order.
 * 2. Keep in mind that descriptions should be readable on 80 wide terminals
 *    so insert appropriate line breaks and newlines.
 * 3. If several command line options should share the same caption and
 *    description they should be declared after another and only the last one
 *    should have the caption and description defined. (ie. -h, -? and -help)
 * 4. When adding an alias put a comment about the date the alias was added.
 * 5. Love thy option system! :) */

struct option_info config_options_info[] = {
	INIT_OPT_TREE("", N_("Configuration system"),
		"config", 0,
		N_("Configuration handling options.")),

	INIT_OPT_INT("config", N_("Comments"),
		"comments", 0, 0, 3, 3,
		N_("Amount of comments automatically written to the config file:\n"
		"0 is no comments are written\n"
		"1 is only the \"blurb\" (name+type) is written\n"
		"2 is only the description is written\n"
		"3 is full comments are written")),

	INIT_OPT_INT("config", N_("Indentation"),
		"indentation", 0, 0, 16, 2,
		N_("Shift width of one indentation level in the configuration\n"
		"file. Zero means that no indentation is performed at all\n"
		"when saving the configuration.")),

	INIT_OPT_INT("config", N_("Saving style"),
		"saving_style", 0, 0, 3, 3,
		N_("Determines what happens when you tell ELinks to save options:\n"
		"0 is only values of current options are altered\n"
		"1 is values of current options are altered and missing options\n"
		"     are added at the end of the file\n"
		"2 is the configuration file is rewritten from scratch\n"
		"3 is values of current options are altered and missing options\n"
		"     CHANGED during this ELinks session are added at the end of\n"
		"     the file")),

	INIT_OPT_BOOL("config", N_("Comments localization"),
		"i18n", 0, 0,
		N_("If set to 1, comments in the configuration file will be\n"
		"translated to the language used by UI. Note that if you have\n"
		"different language set in different terminals, the language\n"
		"used in the configuration file MAY be the same as on the\n"
		"terminal where you saved the file, but it should be generally\n"
		"considered unpredictable.")),


	INIT_OPT_BOOL("config", N_("Saving style warnings"),
		"saving_style_w", 0, 0,
		N_("This is internal option used when displaying a warning about\n"
		"obsolete config.saving_style. You shouldn't touch it.")),

	INIT_OPT_BOOL("config", N_("Show template"),
		"show_template", 0, 0,
		N_("Show _template_ options in autocreated trees in the options\n"
		"manager and save them to the configuration file.")),

	/* Keep options in alphabetical order. */


	INIT_OPT_TREE("", N_("Connections"),
		"connection", OPT_SORT,
		N_("Connection options.")),


	INIT_OPT_BOOL("connection", N_("Asynchronous DNS"),
		"async_dns", 0, 1,
		N_("Use asynchronous DNS resolver?")),

	INIT_OPT_INT("connection", N_("Maximum connections"),
		"max_connections", 0, 1, 16, 10,
		N_("Maximum number of concurrent connections.")),

	INIT_OPT_INT("connection", N_("Maximum connections per host"),
		"max_connections_to_host", 0, 1, 8, 2,
		N_("Maximum number of concurrent connections to a given host.")),

	INIT_OPT_INT("connection", N_("Connection retries"),
		"retries", 0, 0, 16, 3,
		N_("Number of tries to establish a connection.\n"
		   "Zero means try forever.")),

	INIT_OPT_INT("connection", N_("Receive timeout"),
		"receive_timeout", 0, 1, 1800, 120,
		N_("Receive timeout (in seconds).")),

	INIT_OPT_INT("connection", N_("Timeout for non-restartable connections"),
		"unrestartable_receive_timeout", 0, 1, 1800, 600,
		N_("Timeout for non-restartable connections (in seconds).")),

	/* Keep options in alphabetical order. */



	INIT_OPT_TREE("", N_("Document"),
		"document", OPT_SORT,
		N_("Document options.")),

	INIT_OPT_TREE("document", N_("Browsing"),
		"browse", 0,
		N_("Document browsing options (mainly interactivity).")),


	INIT_OPT_TREE("document.browse", N_("Accesskeys"),
		"accesskey", 0,
		N_("Options for handling of the accesskey attribute of the active\n"
		"HTML elements.")),

	INIT_OPT_BOOL("document.browse.accesskey", N_("Automatic links following"),
		"auto_follow", 0, 0,
		N_("Automatically follow a link or submit a form if appropriate\n"
		"accesskey is pressed - this is the standard behaviour, but it's\n"
		"considered dangerous.")),

	INIT_OPT_INT("document.browse.accesskey", N_("Accesskey priority"),
		"priority", 0, 0, 2, 1,
		N_("Priority of 'accesskey' HTML attribute:\n"
		"0 is first try all normal bindings; if it fails, check accesskey\n"
		"1 is first try only frame bindings; if it fails, check accesskey\n"
		"2 is first check accesskey (this can be dangerous)")),


	INIT_OPT_TREE("document.browse", N_("Forms"),
		"forms", 0,
		N_("Options for handling of the forms interaction.")),

	INIT_OPT_BOOL("document.browse.forms", N_("Submit form automatically"),
		"auto_submit", 0, 1,
		N_("Automagically submit a form when enter is pressed with a text\n"
		"field selected.")),

	INIT_OPT_BOOL("document.browse.forms", N_("Confirm submission"),
		"confirm_submit", 0, 1,
		N_("Ask for confirmation when submitting a form.")),


	INIT_OPT_TREE("document.browse", N_("Images"),
		"images", 0,
		N_("Options for handling of images.")),

	INIT_OPT_INT("document.browse.images", N_("Display style for image links"),
		"file_tags", 0, -1, 500, -1,
		N_("Display [target filename] instead of [IMG] as visible image tags:\n"
		"-1    means always display just [IMG]\n"
		"0     means always display full target filename\n"
		"1-500 means display target filename with this maximal length;\n"
		"      if it is longer, the middle is substituted by an asterisk")),

	INIT_OPT_INT("document.browse.images", N_("Image links tagging"),
		"image_link_tagging", 0, 0, 2, 1,
		N_("When to enclose image links:\n"
		"0 means never\n"
		"1 means never if alt or title are provided (old behavior)\n"
		"2 means always")),

	INIT_OPT_STRING("document.browse.images", N_("Image link prefix"),
		"image_link_prefix", 0, "[",
		N_("Prefix string to use to mark image links.")),

	INIT_OPT_STRING("document.browse.images", N_("Image link suffix"),
		"image_link_suffix", 0, "]",
		N_("Suffix string to use to mark image links.")),

	INIT_OPT_BOOL("document.browse.images", N_("Display links to images w/o alt"),
		"show_as_links", 0, 0,
		N_("Display links to images without an alt attribute. If this option\n"
		"is off, these images are completely invisible.")),

	INIT_OPT_BOOL("document.browse.images", N_("Display links to images"),
		"show_any_as_links", 0, 1,
		N_("Display links to any images in the document, regardless of them\n"
		"having an alt attribute or not. If this option is off, the alt\n"
		"attribute contents is shown, but as normal text, not selectable\n"
		"as a link.")),


	INIT_OPT_TREE("document.browse", N_("Links"),
		"links", 0,
		N_("Options for handling of links to other documents.")),

	INIT_OPT_TREE("document.browse.links", N_("Active link"),
		"active_link", 0,
		N_("Options for the active link.")),

	INIT_OPT_TREE("document.browse.links.active_link", N_("Colors"),
		"colors", 0,
		N_("Active link colors.")),

	INIT_OPT_COLOR("document.browse.links.active_link.colors", N_("Background color"),
		"background", 0, "#0000ff",
		N_("Default background color.")),

	INIT_OPT_COLOR("document.browse.links.active_link.colors", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_BOOL("document.browse.links.active_link", N_("Enable color"),
		"enable_color", 0, 0,
		N_("Enable use of the active link background and text color\n"
		"settings instead of browse.links from the document.")),

	INIT_OPT_BOOL("document.browse.links.active_link", N_("Bold"),
		"bold", 0, 0,
		N_("Make the active link text bold.")),

	INIT_OPT_BOOL("document.browse.links.active_link", N_("Invert colors"),
		"invert", 0, 1,
		N_("Invert the fore- and background color so the link "
		"stands out.")),

	INIT_OPT_BOOL("document.browse.links.active_link", N_("Underline"),
		"underline", 0, 0,
		N_("Underline the active link.")),


	INIT_OPT_BOOL("document.browse.links", N_("Directory highlighting"),
		"color_dirs", 0, 1,
		N_("Highlight links to directories in FTP and local directory listing.")),

	INIT_OPT_BOOL("document.browse.links", N_("Number links"),
		"numbering", 0, 0,
		N_("Display numbers next to the links.")),

	INIT_OPT_BOOL("document.browse.links", N_("Use tabindex"),
		"use_tabindex", 0, 1,
		N_("Whether to navigate links using tabindex.")),

	INIT_OPT_INT("document.browse.links", N_("Number keys select links"),
		"number_keys_select_link", 0, 0, 2, 1,
		N_("Number keys select links rather than specify command prefixes. This\n"
		"is a tristate:\n"
		"0 means never\n"
		"1 means if document.browse.links.numbering = 1\n"
		"2 means always")),

	/* TODO - this is somehow implemented by ff, but disabled
	 * for now as it doesn't work. */
	INIT_OPT_BOOL("document.browse.links", N_("Wrap-around links cycling"),
		"wraparound", /* 0 */ 0, 0,
		N_("When pressing 'down' on the last link, jump at the first one, and\n"
		"vice versa.")),


	INIT_OPT_TREE("document.browse", N_("Searching"),
		"search", 0,
		N_("Options for searching.")),

	INIT_OPT_BOOL("document.browse.search", N_("Case sensitivity"),
		"case", 0, 0,
		N_("Whether the search should match the document text while maintaining\n"
		"case sensitivity.")),

	INIT_OPT_BOOL("document.browse.search", N_("Overlapping matches"),
		"overlap", 0, 0,
		N_("Let matches overlap (only relevant to regex search).\n"
		"0 means matches should not overlap.\n"
		"1 means matches can overlap (CAUTION: can be cpu intensive with\n"
		"  some expressions!)")),

#ifdef HAVE_REGEX_H
	INIT_OPT_INT("document.browse.search", N_("Regular expressions"),
		"regex", 0, 0, 2, 0,
		N_("Enable searching with regular expressions.\n"
		"0 for plain text searching.\n"
		"1 for basic regular expression searches.\n"
		"2 for extended regular expression searches.")),
#else
	INIT_OPT_INT("document.browse.search", N_("Regular expressions"),
		"regex", 0, 0, 2, 0,
		N_("Enable searching with regular expressions.\n"
		"0 for plain text searching.\n"
		"1 for basic regular expression searches. (DISABLED)\n"
		"2 for extended regular expression searches. (DISABLED)")),
#endif

	INIT_OPT_BOOL("document.browse.search", N_("Show search hit top or bottom dialogs"),
		"show_hit_top_bottom", 0, 1,
		N_("Whether to show a dialog when the search hits top or bottom of the\n"
		"document.")),


	INIT_OPT_INT("document.browse", N_("Horizontal text margin"),
		"margin_width", 0, 0, 9, 3,
		N_("Horizontal text margin.")),

	INIT_OPT_BOOL("document.browse", N_("Document meta refresh"),
		"refresh", 0, 0,
		N_("Automatically follow document-specified refresh directives ('<meta>\n"
		"refresh' tags). Web-page authors use these to instruct the browser\n"
		"to reload a document at a given interval or to load another page.")),

	INIT_OPT_INT("document.browse", N_("Scroll margin"),
		"scroll_margin", 0, 0, 20, 3,
		N_("Size of the virtual margin - when you click inside of that margin,\n"
		"document scrolls in that direction.")),

	INIT_OPT_INT("document.browse", N_("Scroll step"),
		"scroll_step", 0, 0, MAXINT, 2,
		N_("Number of lines to scroll when a key bound to scroll-up or scroll-\n"
		"down is pressed and no prefix was given.")),

	INIT_OPT_BOOL("document.browse", N_("Tables navigation order"),
		"table_move_order", 0, 0,
		N_("Move by columns in table, instead of rows.")),

	/* Keep options in alphabetical order. */



	INIT_OPT_TREE("document", N_("Cache"),
		"cache", 0,
		N_("Cache options.")),

	INIT_OPT_BOOL("document.cache", N_("Cache information about redirects"),
		"cache_redirects", 0, 0,
		N_("Cache even redirects sent by server (usually thru HTTP by a 302\n"
		"HTTP code and a Location header). This was the original behaviour\n"
		"for a quite some time, but it causes problems in a situation very\n"
		"common to various web login systems - frequently, when accessing\n"
		"certain location, they will redirect you to a login page if they\n"
		"don't receive an auth cookie, the login page then gives you the\n"
		"cookie and redirects you back to the original page, but there you\n"
		"have already cached redirect back to the login page! If this\n"
		"option has value of 0, this malfunction is fixed, but occasionally\n"
		"you may get superfluous (depends on how you take it ;-) requests to\n"
		"the server. If this option has value of 1, experienced users can\n"
		"still workaround it by clever combination of usage of reload,\n"
		"jumping around in session history and hitting ctrl+enter.\n"
		"Note that this option is checked when retrieving the information\n"
		"from cache, not when saving it to cache - thus if you will enable\n"
		"it, even previous redirects will be taken from cache instead of\n"
		"asking the server.")),

	INIT_OPT_BOOL("document.cache", N_("Ignore cache-control info from server"),
		"ignore_cache_control", 0, 1,
		N_("Ignore Cache-Control and Pragma server headers.\n"
		"When set, the document is cached even with 'Cache-Control: no-cache'.")),

	INIT_OPT_TREE("document.cache", N_("Formatted documents"),
		"format", 0,
		N_("Format cache options.")),

	INIT_OPT_INT("document.cache.format", N_("Number"),
		"size", 0, 0, 256, 5,
		N_("Number of cached formatted pages.")),

	INIT_OPT_TREE("document.cache", N_("Memory cache"),
		"memory", 0,
		N_("Memory cache options.")),

	INIT_OPT_LONG("document.cache.memory", N_("Size"),
		"size", 0, 0, MAXLONG, 1048576,
		N_("Memory cache size (in bytes).")),



	INIT_OPT_TREE("document", N_("Charset"),
		"codepage", 0,
		N_("Charset options.")),

	INIT_OPT_CODEPAGE("document.codepage", N_("Default codepage"),
		"assume", 0, "ISO-8859-1",
		N_("Default document codepage.")),

	INIT_OPT_BOOL("document.codepage", N_("Ignore charset info from server"),
		"force_assumed", 0, 0,
		N_("Ignore charset info sent by server.")),



	INIT_OPT_TREE("document", N_("Default color settings"),
		"colors", 0,
		N_("Default document color settings.")),


	INIT_OPT_COLOR("document.colors", N_("Text color"),
		"text", 0, "#bfbfbf",
		N_("Default text color.")),

	INIT_OPT_COLOR("document.colors", N_("Background color"),
		"background", 0, "#000000",
		N_("Default background color.")),

	INIT_OPT_COLOR("document.colors", N_("Link color"),
		"link", 0, "#0000ff",
		N_("Default link color.")),

	INIT_OPT_COLOR("document.colors", N_("Visited-link color"),
		"vlink", 0, "#ffff00",
		N_("Default visited link color.")),

	INIT_OPT_COLOR("document.colors", N_("Directory color"),
		"dirs", 0, "#ffff00",
		N_("Default directory color.\n"
		"See document.browse.links.color_dirs option.")),

	INIT_OPT_BOOL("document.colors", N_("Allow dark colors on black background"),
		"allow_dark_on_black", 0, 0,
		N_("Allow dark colors on black background.")),

	INIT_OPT_BOOL("document.colors", N_("Ensure contrast"),
		"ensure_contrast", 0, 1,
		N_("Makes sure that the back- and foreground color is never "
		"equal.")),

	/* If you change this please also change ACT_DOCUMENT_COLORS action
	 * handling. */
	INIT_OPT_INT("document.colors", N_("Use document-specified colors"),
		"use_document_colors", 0, 0, 2, 2,
		N_("Use colors specified in document:\n"
		"0 is use always the default settings\n"
		"1 is use document colors if available, except background\n"
		"2 is use document colors, including background. This can\n"
		"  look really impressive mostly, but few sites look really\n"
		"  ugly there (unfortunately including slashdot (but try to\n"
		"  let him serve you that 'plain' version and the world will\n"
		"  suddenly become a much more happy place for life)). Note\n"
		"  that obviously if the background isn't black, it will\n"
		"  break transparency, if you have it enabled for your terminal\n"
		"  and on your terminal.")),


	/* Keep options in alphabetical order. */



	INIT_OPT_TREE("document", N_("Downloading"),
		"download", 0,
		N_("Options regarding files downloading and handling.")),

	/* Compatibility alias. Added: 2003-05-07, 0.5pre1.CVS. */
	INIT_OPT_ALIAS("document.download", NULL,
		"default_mime_type", 0, "mime.default_type",
		N_("This option is deprecated and will be removed very soon.\n"
		"Please use the mime.default_type option instead.")),

	INIT_OPT_STRING("document.download", N_("Default download directory"),
		"directory", 0, "./",
		N_("Default download directory.")),

	INIT_OPT_BOOL("document.download", N_("Set original time"),
		"set_original_time", 0, 0,
		N_("Set the timestamp of each downloaded file to the timestamp\n"
		"stored on the server.")),

	/* Does automatic resuming make sense as an option? */
	INIT_OPT_INT("document.download", N_("Prevent overwriting"),
		"overwrite", 0, 0, 2, 2,
		N_("Prevent overwriting the local files:\n"
		"0 is files will silently be overwritten.\n"
		"1 is add a suffix .{number} (for example '.1') to the name.\n"
		"2 is ask the user.")),

	INIT_OPT_INT("document.download", N_("Notify download completion by bell"),
		"notify_bell", 0, 0, 2, 0,
		N_("Audio notification when download is completed:\n"
		"0 is never.\n"
		"1 is when background notification is active.\n"
		"2 is always")),


	INIT_OPT_TREE("document", N_("Dump output"),
		"dump", 0,
		N_("Dump output options.")),

	INIT_OPT_CODEPAGE("document.dump", N_("Codepage"),
		"codepage", 0, "us-ascii",
		N_("Codepage used in dump output.")),

	INIT_OPT_INT("document.dump", N_("Width"),
		"width", 0, 40, 512, 80,
		N_("Width of screen in characters when dumping a HTML document.")),



	INIT_OPT_TREE("document", N_("History"),
		"history", OPT_SORT,
		N_("History options.")),

	INIT_OPT_BOOL("document.history", N_("Keep unhistory"),
		"keep_unhistory", 0, 1,
		N_("Keep unhistory (\"forward history\")?")),



	INIT_OPT_TREE("document", N_("HTML rendering"),
		"html", 0,
		N_("Options concerning the display of HTML pages.")),

	INIT_OPT_BOOL("document.html", N_("Display frames"),
		"display_frames", 0, 1,
		N_("Display frames.")),

	INIT_OPT_BOOL("document.html", N_("Display tables"),
		"display_tables", 0, 1,
		N_("Display tables.")),

	INIT_OPT_BOOL("document.html", N_("Expand table columns"),
		"expand_table_columns", 0, 0,
		N_("Enable to make table rendering expand table columns so cells\n"
		"are more correctly vertically aligned with respect to the background\n"
		"colors, but less correctly vertically aligned with respect to the\n"
		"overall layout. Only useful if you also have\n"
		"document.colors.use_document_colors set to 2. Experimental.")),

	INIT_OPT_BOOL("document.html", N_("Display subscripts"),
		"display_subs", 0, 1,
		N_("Display subscripts (as [thing]).")),

	INIT_OPT_BOOL("document.html", N_("Display superscripts"),
		"display_sups", 0, 1,
		N_("Display superscripts (as ^thing).")),

	INIT_OPT_INT("document.html", N_("Rendering of html link element"),
		"link_display", 0, 0, 5, 2,
		N_("Html link rendering verbosity:\n"
		"0 is nothing\n"
		"1 is title\n"
		"2 is name in addition\n"
		"3 is hreflang in addition\n"
		"4 is type in addition\n"
		"5 is everything")),

	INIT_OPT_BOOL("document.html", N_("Underline links"),
		"underline_links", 0, 0,
		N_("Underline links.")),

	/* Keep options in alphabetical order. */



	INIT_OPT_TREE("", N_("Protocols"),
		"protocol", OPT_SORT,
		N_("Protocol specific options.")),

	INIT_OPT_TREE("protocol", N_("HTTP"),
		"http", 0,
		N_("HTTP-specific options.")),


	INIT_OPT_TREE("protocol.http", N_("Server bug workarounds"),
		"bugs", 0,
		N_("Server-side HTTP bugs workarounds.")),

	INIT_OPT_BOOL("protocol.http.bugs", N_("Do not send Accept-Charset"),
		"accept_charset", 0, 1,
		N_("The Accept-Charset header is quite long and sending it can trigger\n"
		"bugs in some rarely found servers.")),

	INIT_OPT_BOOL("protocol.http.bugs", N_("Allow blacklisting"),
		"allow_blacklist", 0, 1,
		N_("Allow blacklisting of buggy servers.")),

	INIT_OPT_BOOL("protocol.http.bugs", N_("Broken 302 redirects"),
		"broken_302_redirect", 0, 1,
		N_("Broken 302 redirect (violates RFC but compatible with Netscape).\n"
		"This is a problem for a lot of web discussion boards and the like.\n"
		"If they will do strange things to you, try to play with this.")),

	INIT_OPT_BOOL("protocol.http.bugs", N_("No keepalive after POST requests"),
		"post_no_keepalive", 0, 0,
		N_("Disable keepalive connection after POST request.")),

	INIT_OPT_BOOL("protocol.http.bugs", N_("Use HTTP/1.0"),
		"http10", 0, 0,
		N_("Use HTTP/1.0 protocol instead of HTTP/1.1.")),

	INIT_OPT_TREE("protocol.http", N_("Proxy configuration"),
		"proxy", 0,
		N_("HTTP proxy configuration.")),

	INIT_OPT_STRING("protocol.http.proxy", N_("Host and port-number"),
		"host", 0, "",
		N_("Host and port-number (host:port) of the HTTP proxy, or blank.\n"
		"If it's blank, HTTP_PROXY environment variable is checked as well.")),

	INIT_OPT_STRING("protocol.http.proxy", N_("Username"),
		"user", 0, "",
		N_("Proxy authentication username.")),

	INIT_OPT_STRING("protocol.http.proxy", N_("Password"),
		"passwd", 0, "",
		N_("Proxy authentication password.")),


	INIT_OPT_TREE("protocol.http", N_("Referer sending"),
		"referer", 0,
		N_("HTTP referer sending options. HTTP referer is a special header\n"
		"sent in the HTTP requests, which is supposed to contain the previous\n"
		"page visited by the browser. This way, the server can know what link\n"
		"did you follow when accessing that page. However, this behaviour\n"
		"can unfortunately considerably affect privacy and can lead even to a\n"
		"security problem on some badly designed web pages.")),

	INIT_OPT_INT("protocol.http.referer", N_("Policy"),
		"policy", 0,
		REFERER_NONE, REFERER_TRUE, REFERER_SAME_URL,
		N_("Mode of sending HTTP referer:\n"
		"0 is send no referer\n"
		"1 is send current URL as referer\n"
		"2 is send fixed fake referer\n"
		"3 is send previous URL as referer (correct, but insecure)\n")),

	INIT_OPT_STRING("protocol.http.referer", N_("Fake referer URL"),
		"fake", 0, "",
		N_("Fake referer to be sent when policy is 2.")),


	INIT_OPT_STRING("protocol.http", N_("Send Accept-Language header"),
		"accept_language", 0, "",
		N_("Send Accept-Language header.")),

	INIT_OPT_BOOL("protocol.http", N_("Use UI language as Accept-Language"),
		"accept_ui_language", 0, 1,
		N_("Request localised versions of documents from web-servers (using the\n"
		"Accept-Language header) using the language you have configured for\n"
		"ELinks' user-interface. Note that some see this as a potential security\n"
		"risk because it tells web-masters about your preference in language.")),

	INIT_OPT_BOOL("protocol.http", N_("Activate HTTP TRACE debugging"),
		"trace", 0, 0,
		N_("If active, all HTTP requests are sent with TRACE as their method\n"
		"rather than GET or POST. This is useful for debugging of both ELinks\n"
		"and various server-side scripts --- the server only returns the client's\n"
		"request back to the client verbatim. Note that this type of request may\n"
		"not be enabled on all servers.")),

	INIT_OPT_STRING("protocol.http", N_("User-agent identification"),
		"user_agent", 0, "ELinks (%v; %s; %t)",
		N_("Change the User Agent ID. That means identification string, which\n"
		"is sent to HTTP server when a document is requested.\n"
		"%v in the string means ELinks version\n"
		"%s in the string means system identification\n"
		"%t in the string means size of the terminal\n"
		"Use \" \" if you don't want any User-Agent header to be sent at all.")),



	INIT_OPT_TREE("protocol", N_("FTP"),
		"ftp", 0,
		N_("FTP specific options.")),

	INIT_OPT_TREE("protocol.ftp", N_("Proxy configuration"),
		"proxy", 0,
		N_("FTP proxy configuration.")),

	INIT_OPT_STRING("protocol.ftp.proxy", N_("Host and port-number"),
		"host", 0, "",
		N_("Host and port-number (host:port) of the FTP proxy, or blank.\n"
		"If it's blank, FTP_PROXY environment variable is checked as well.")),

	INIT_OPT_STRING("protocol.ftp", N_("Anonymous password"),
		"anon_passwd", 0, "some@host.domain",
		N_("FTP anonymous password to be sent.")),

	INIT_OPT_BOOL("protocol.ftp", N_("Use passive mode (IPv4)"),
		"use_pasv", 0, 1,
		N_("Use PASV instead of PORT (passive vs active mode, IPv4 only).")),
#ifdef IPV6
	INIT_OPT_BOOL("protocol.ftp", N_("Use passive mode (IPv6)"),
		"use_epsv", 0, 0,
		N_("Use EPSV instead of EPRT (passive vs active mode, IPv6 only).")),
#else
	INIT_OPT_BOOL("protocol.ftp", N_("Use passive mode (IPv6)"),
		"use_epsv", 0, 0,
		N_("Use EPSV instead of EPRT (passive vs active mode, IPv6 only).\n"
		"Works only with IPv6 enabled, so nothing interesting for you.")),
#endif



	INIT_OPT_TREE("protocol", N_("Local files"),
		"file", 0,
		N_("Options specific to local browsing.")),

	INIT_OPT_BOOL("protocol.file", N_("Allow reading special files"),
		"allow_special_files", 0, 0,
		N_("Allow reading from non-regular files? (DANGEROUS - reading\n"
		"/dev/urandom or /dev/zero can ruin your day!)")),

	INIT_OPT_BOOL("protocol.file", N_("Try encoding extensions"),
		"try_encoding_extensions", 0, 1,
		N_("When set, if we can't open a file named 'filename', we'll try\n"
		"to open 'filename' with some encoding extension appended\n"
		"(ie. 'filename.gz'); it depends on the supported encodings.")),

	INIT_OPT_BOOL("protocol.file", N_("Show hidden files in directory listing"),
		"show_hidden_files", 0, 1,
		N_("Show hidden files in directory listing ?\n"
		   "If set to false, files with name starting with a dot will be\n"
		   "hidden in local directories listing.")),


	INIT_OPT_TREE("protocol", N_("User protocols"),
		"user", OPT_AUTOCREATE,
		N_("User protocols. Options in this tree specify external\n"
		"handlers for the appropriate protocols. Ie.\n"
		"protocol.user.mailto.unix.")),

	/* FIXME: Poorly designed options structure. Ought to be able to specify
	 * need_slashes, free_form and similar options as well :-(. --pasky */

	/* Basically, it looks like protocol.user.mailto.win32 = "blah" */

	INIT_OPT_TREE("protocol.user", NULL,
		"_template_", OPT_AUTOCREATE,
		N_("Handler (external program) for this protocol. Name the\n"
		"options in this tree after your system (ie. unix, unix-xwin).")),

	INIT_OPT_STRING("protocol.user._template_", NULL,
		"_template_", 0, "",
		N_("Handler (external program) for this protocol and system.\n"
		"%h in the string means hostname (or email address)\n"
		"%p in the string means port\n"
		"%d in the string means path (everything after the port)\n"
		"%s in the string means subject (?subject=<this>)\n"
		"%u in the string means the whole URL")),


	/* Start of mailcap compatibility aliases:
	 * Added: 2003-05-07, 0.5pre0.CVS.
	 * Estimated due time: ? */
	INIT_OPT_TREE("protocol", N_("Mailcap"),
		"mailcap", 0,
		N_("Options for mailcap support. (Deprecated. Please use\n"
		"mime.mailcap instead)")),

	INIT_OPT_ALIAS("protocol.mailcap", NULL,
		"enable", 0, "mime.mailcap.enable",
		N_("This option is deprecated and will be removed very soon.\n"
		"Please use the mime.mailcap.enable option instead.")),

	INIT_OPT_ALIAS("protocol.mailcap", NULL,
		"path", 0, "mime.mailcap.path",
		N_("This option is deprecated and will be removed very soon.\n"
		"Please use the mime.mailcap.path option instead.")),

	INIT_OPT_ALIAS("protocol.mailcap", NULL,
		"ask", 0, "mime.mailcap.ask",
		N_("This option is deprecated and will be removed very soon.\n"
		"Please use the mime.mailcap.ask option instead.")),

	INIT_OPT_INT("protocol.mailcap", N_("Type query string"),
		"description", 0, 0, 2, 0,
		N_("This option is deprecated and will be removed very soon.\n")),

	INIT_OPT_ALIAS("protocol.mailcap", NULL,
		"prioritize", 0, "mime.mailcap.prioritize",
		N_("This option is deprecated and will be removed very soon.\n"
		"Please use the mime.mailcap.prioritize option instead.")),


	INIT_OPT_STRING("protocol", N_("No-proxy domains"),
		"no_proxy", 0, "",
		N_("Comma separated list of domains for which the proxy (HTTP/FTP)\n"
		"should be disabled. Optionally, a port can be specified for some\n"
		"domains as well. If it's blank, NO_PROXY environment variable is\n"
		"checked as well.")),

	/* Keep options in alphabetical order. */



	INIT_OPT_TREE("", N_("Terminals"),
		"terminal", OPT_AUTOCREATE,
		N_("Terminal options.")),

	INIT_OPT_TREE("terminal", NULL,
		"_template_", 0,
		N_("Options specific to this terminal type (according to $TERM value).")),

	INIT_OPT_INT("terminal._template_", N_("Border type"),
		"type", 0, 0, 3, 0,
		N_("Terminal type; matters mostly only when drawing frames and\n"
		"dialog box borders:\n"
		"0 is dumb terminal type, ASCII art\n"
		"1 is VT100, simple but portable\n"
		"2 is Linux, you get double frames and other goodies\n"
		"3 is KOI-8")),

	INIT_OPT_BOOL("terminal._template_", N_("Switch fonts for line drawing"),
		"m11_hack", 0, 0,
		N_("Switch fonts when drawing lines, enabling both local characters\n"
		"and lines working at the same time. Makes sense only with linux\n"
		"terminal.")),

	INIT_OPT_BOOL("terminal._template_", N_("I/O in UTF8"),
		"utf_8_io", 0, 0,
		N_("Enable I/O in UTF8 for Unicode terminals. Note that currently,\n"
		"only the subset of UTF8 according to terminal codepage is used.")),

	INIT_OPT_BOOL("terminal._template_", N_("Restrict frames in cp850/852"),
		"restrict_852", 0, 0,
		N_("Restrict the characters used when drawing lines. Makes sense\n"
		"only with linux terminals using the cp850/852 character sets.")),

	INIT_OPT_BOOL("terminal._template_", N_("Block cursor"),
		"block_cursor", 0, 0,
		N_("Move cursor to bottom right corner when done drawing.\n"
		"This is particularly useful when we have a block cursor,\n"
		"so that inversed text is displayed correctly.")),

	INIT_OPT_INT("terminal._template_", N_("Color mode"),
		"colors", 0, 0, 2, 0,
		N_("The color mode controls what colors are used and how they are\n"
		"output to the terminal. The color modes are:\n"
		"0 is mono mode, only 2 colors are used\n"
		"1 is 16 color mode, uses the common ANSI colors\n"
		"2 is 256 color mode, requires that xterm-256color TERM is used\n")),

	INIT_OPT_BOOL("terminal._template_", N_("Enable transparency"),
		"transparency", 0, 1,
		N_("If we should not set the background to black. This is particularly\n"
		"useful when we have a terminal (typically in some windowing\n"
		"environment) with a background image or a transparent background -\n"
		"it will be visible in ELinks as well. Note that this option makes\n"
		"sense only when colors are enabled.")),

	INIT_OPT_BOOL("terminal._template_", N_("Use underline"),
		"underline", 0, 0,
		N_("If we should use underline or enhance the color instead.")),

	INIT_OPT_CODEPAGE("terminal._template_", N_("Codepage"),
		"charset", 0, "us-ascii",
		N_("Codepage of charset used for displaying content on terminal.")),

	/* Keep options in alphabetical order. */



	INIT_OPT_TREE("", N_("User interface"),
		"ui", 0,
		N_("User interface options.")),



	INIT_OPT_TREE("ui", N_("Color settings"),
		"colors", 0,
		N_("Default user interface color settings.")),


	/* ========================================================== */
	/* ============= BORING PART (colors) START ================= */
	/* ========================================================== */
	/* XXX: All bfu colors needs to have both 'text' and 'background'
	 *	options even if it is not used. get_bfu_color() depends
	 *	on it. */

	INIT_OPT_TREE("ui.colors", N_("Color terminals"),
		"color", 0,
		N_("Color settings for color terminal.")),


	INIT_OPT_TREE("ui.colors.color", N_("Main menu bar"),
		"mainmenu", 0,
		N_("Main menu bar colors.")),

	INIT_OPT_TREE("ui.colors.color.mainmenu", N_("Unselected main menu bar item"),
		"normal", 0,
		N_("Unselected main menu bar item colors.")),

	INIT_OPT_COLOR("ui.colors.color.mainmenu.normal", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.color.mainmenu.normal", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.color.mainmenu", N_("Selected main menu bar item"),
		"selected", 0,
		N_("Selected main menu bar item colors.")),

	INIT_OPT_COLOR("ui.colors.color.mainmenu.selected", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.color.mainmenu.selected", N_("Background color"),
		"background", 0, "green",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.color.mainmenu", N_("Hotkey"),
		"hotkey", 0,
		N_("Main menu hotkey colors.")),

	INIT_OPT_TREE("ui.colors.color.mainmenu.hotkey", N_("Unselected hotkey"),
		"normal", 0,
		N_("Main menu unselected hotkey colors.")),

	INIT_OPT_TREE("ui.colors.color.mainmenu.hotkey", N_("Selected hotkey"),
		"selected", 0,
		N_("Main menu selected hotkey colors.")),

	INIT_OPT_COLOR("ui.colors.color.mainmenu.hotkey.normal", N_("Text color"),
		"text", 0, "darkred",
		N_("Main menu unselected hotkey default text color.")),

	INIT_OPT_COLOR("ui.colors.color.mainmenu.hotkey.normal", N_("Background color"),
		"background", 0, "white",
		N_("Main menu unselected hotkey default background color.")),

	INIT_OPT_COLOR("ui.colors.color.mainmenu.hotkey.selected", N_("Text color"),
		"text", 0, "darkred",
		N_("Main menu selected hotkey text color.")),

	INIT_OPT_COLOR("ui.colors.color.mainmenu.hotkey.selected", N_("Background color"),
		"background", 0, "green",
		N_("Main menu selected hotkey default background color.")),


	INIT_OPT_TREE("ui.colors.color", N_("Menu bar"),
		"menu", 0,
		N_("Menu bar colors.")),

	INIT_OPT_TREE("ui.colors.color.menu", N_("Unselected menu item"),
		"normal", 0,
		N_("Unselected menu item colors.")),

	INIT_OPT_COLOR("ui.colors.color.menu.normal", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.color.menu.normal", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.color.menu", N_("Selected menu item"),
		"selected", 0,
		N_("Selected menu item colors.")),

	INIT_OPT_COLOR("ui.colors.color.menu.selected", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.color.menu.selected", N_("Background color"),
		"background", 0, "green",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.color.menu", N_("Marked menu item"),
		"marked", 0,
		N_("Marked menu item colors.")),

	INIT_OPT_COLOR("ui.colors.color.menu.marked", N_("Text color"),
		"text", 0, "red",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.color.menu.marked", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.color.menu", N_("Hotkey"),
		"hotkey", 0,
		N_("Menu item hotkey colors.")),

	INIT_OPT_TREE("ui.colors.color.menu.hotkey", N_("Unselected hotkey"),
		"normal", 0,
		N_("Menu item unselected hotkey colors.")),

	INIT_OPT_TREE("ui.colors.color.menu.hotkey", N_("Selected hotkey"),
		"selected", 0,
		N_("Menu item selected hotkey colors.")),

	INIT_OPT_COLOR("ui.colors.color.menu.hotkey.normal", N_("Text color"),
		"text", 0, "darkred",
		N_("Menu item unselected hotkey default text color.")),

	INIT_OPT_COLOR("ui.colors.color.menu.hotkey.normal", N_("Background color"),
		"background", 0, "white",
		N_("Menu item unselected hotkey default background color.")),

	INIT_OPT_COLOR("ui.colors.color.menu.hotkey.selected", N_("Text color"),
		"text", 0, "darkred",
		N_("Menu item selected hotkey default text color.")),

	INIT_OPT_COLOR("ui.colors.color.menu.hotkey.selected", N_("Background color"),
		"background", 0, "green",
		N_("Menu item selected hotkey background color.")),

	INIT_OPT_TREE("ui.colors.color.menu", N_("Menu frame"),
		"frame", 0,
		N_("Menu frame colors.")),

	INIT_OPT_COLOR("ui.colors.color.menu.frame", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.color.menu.frame", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),


	INIT_OPT_TREE("ui.colors.color", N_("Dialog"),
		"dialog", 0,
		N_("Dialog colors.")),

	/* Compatibility alias: added by jonas at 2003-10-18, 0.5pre7.CVS. */
	INIT_OPT_ALIAS("ui.colors.color.dialog", NULL,
		"background", 0, "ui.colors.color.dialog.generic.background",
		N_("This option is deprecated and will be removed very soon.\n"
		"Please use ui.colors.color.dialog.generic.background instead.")),

	INIT_OPT_TREE("ui.colors.color.dialog", N_("Generic"),
		"generic", 0,
		N_("Generic dialog colors.")),

	INIT_OPT_COLOR("ui.colors.color.dialog.generic", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.color.dialog.generic", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.color.dialog", N_("Frame"),
		"frame", 0,
		N_("Dialog frame colors.")),

	INIT_OPT_COLOR("ui.colors.color.dialog.frame", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.color.dialog.frame", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.color.dialog", N_("Title"),
		"title", 0,
		N_("Dialog title colors.")),

	INIT_OPT_COLOR("ui.colors.color.dialog.title", N_("Text color"),
		"text", 0, "darkred",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.color.dialog.title", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.color.dialog", N_("Text"),
		"text", 0,
		N_("Dialog text colors.")),

	INIT_OPT_COLOR("ui.colors.color.dialog.text", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.color.dialog.text", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.color.dialog", N_("Checkbox"),
		"checkbox", 0,
		N_("Dialog checkbox colors.")),

	INIT_OPT_COLOR("ui.colors.color.dialog.checkbox", N_("Text color"),
		"text", 0, "darkred",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.color.dialog.checkbox", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.color.dialog", N_("Checkbox label"),
		"checkbox-label", 0,
		N_("Dialog checkbox label colors.")),

	INIT_OPT_COLOR("ui.colors.color.dialog.checkbox-label", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.color.dialog.checkbox-label", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.color.dialog", N_("Button"),
		"button", 0,
		N_("Dialog button colors.")),

	INIT_OPT_COLOR("ui.colors.color.dialog.button", N_("Text color"),
		"text", 0, "white",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.color.dialog.button", N_("Background color"),
		"background", 0, "blue",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.color.dialog", N_("Selected button"),
		"button-selected", 0,
		N_("Dialog selected button colors.")),

	INIT_OPT_COLOR("ui.colors.color.dialog.button-selected", N_("Text color"),
		"text", 0, "yellow",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.color.dialog.button-selected", N_("Background color"),
		"background", 0, "green",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.color.dialog", N_("Text field"),
		"field", 0,
		N_("Dialog text field colors.")),

	INIT_OPT_COLOR("ui.colors.color.dialog.field", N_("Text color"),
		"text", 0, "white",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.color.dialog.field", N_("Background color"),
		"background", 0, "blue",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.color.dialog", N_("Text field text"),
		"field-text", 0,
		N_("Dialog field text colors.")),

	INIT_OPT_COLOR("ui.colors.color.dialog.field-text", N_("Text color"),
		"text", 0, "yellow",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.color.dialog.field-text", N_("Background color"),
		"background", 0, "blue",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.color.dialog", N_("Meter"),
		"meter", 0,
		N_("Dialog meter colors.")),

	INIT_OPT_COLOR("ui.colors.color.dialog.meter", N_("Text color"),
		"text", 0, "white",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.color.dialog.meter", N_("Background color"),
		"background", 0, "blue",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.color.dialog", N_("Shadow"),
		"shadow", 0,
		N_("Dialog shadow colors (see ui.shadows option).")),

	INIT_OPT_COLOR("ui.colors.color.dialog.shadow", N_("Background color"),
		"background", 0, "black",
		N_("Default background color.")),

	INIT_OPT_COLOR("ui.colors.color.dialog.shadow", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_TREE("ui.colors.color", N_("Title bar"),
		"title", 0,
		N_("Title bar colors.")),

	INIT_OPT_TREE("ui.colors.color.title", N_("Generic title bar"),
		"title-bar", 0,
		N_("Generic title bar colors.")),

	INIT_OPT_COLOR("ui.colors.color.title.title-bar", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.color.title.title-bar", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.color.title", N_("Title bar text"),
		"title-text", 0,
		N_("Title bar text colors.")),

	INIT_OPT_COLOR("ui.colors.color.title.title-text", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.color.title.title-text", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),


	INIT_OPT_TREE("ui.colors.color", N_("Status bar"),
		"status", 0,
		N_("Status bar colors.")),

	INIT_OPT_TREE("ui.colors.color.status", N_("Generic status bar"),
		"status-bar", 0,
		N_("Generic status bar colors.")),

	INIT_OPT_COLOR("ui.colors.color.status.status-bar", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.color.status.status-bar", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.color.status", N_("Status bar text"),
		"status-text", 0,
		N_("Status bar text colors.")),

	INIT_OPT_COLOR("ui.colors.color.status.status-text", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.color.status.status-text", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),


	INIT_OPT_TREE("ui.colors.color", N_("Tabs bar"),
		"tabs", 0,
		N_("Tabs bar colors.")),

	INIT_OPT_TREE("ui.colors.color.tabs", N_("Unselected tab"),
		"normal", 0,
		N_("Unselected tab colors.")),

	INIT_OPT_COLOR("ui.colors.color.tabs.normal", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.color.tabs.normal", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.color.tabs", N_("Loading tab"),
		"loading", 0,
		N_("Tab colors for tabs that are loading in the background.")),

	INIT_OPT_COLOR("ui.colors.color.tabs.loading", N_("Text color"),
		"text", 0, "darkred",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.color.tabs.loading", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.color.tabs", N_("Selected tab"),
		"selected", 0,
		N_("Selected tab colors.")),

	INIT_OPT_COLOR("ui.colors.color.tabs.selected", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.color.tabs.selected", N_("Background color"),
		"background", 0, "green",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.color.tabs", N_("Tab separator"),
		"separator", 0,
		N_("Tab separator colors.")),

	INIT_OPT_COLOR("ui.colors.color.tabs.separator", N_("Text color"),
		"text", 0, "green",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.color.tabs.separator", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.color", N_("Searched strings"),
		"searched", 0,
		N_("Searched string highlight colors.")),

	INIT_OPT_COLOR("ui.colors.color.searched", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.color.searched", N_("Background color"),
		"background", 0, "lime",
		N_("Default background color.")),



	INIT_OPT_TREE("ui.colors", N_("Non-color terminals"),
		"mono", 0,
		N_("Color settings for non-color terminal.")),


	INIT_OPT_TREE("ui.colors.mono", N_("Main menu bar"),
		"mainmenu", 0,
		N_("Main menu bar colors.")),

	INIT_OPT_TREE("ui.colors.mono.mainmenu", N_("Unselected menu bar item"),
		"normal", 0,
		N_("Unselected menu bar item colors.")),

	INIT_OPT_COLOR("ui.colors.mono.mainmenu.normal", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.mainmenu.normal", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.mono.mainmenu", N_("Selected menu bar item"),
		"selected", 0,
		N_("Selected menu bar item colors.")),

	INIT_OPT_COLOR("ui.colors.mono.mainmenu.selected", N_("Text color"),
		"text", 0, "gray",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.mainmenu.selected", N_("Background color"),
		"background", 0, "black",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.mono.mainmenu", N_("Hotkey"),
		"hotkey", 0,
		N_("Main menu hotkey colors.")),

	INIT_OPT_TREE("ui.colors.mono.mainmenu.hotkey", N_("Unselected hotkey"),
		"normal", 0,
		N_("Main menu unselected hotkey colors.")),

	INIT_OPT_TREE("ui.colors.mono.mainmenu.hotkey", N_("Selected hotkey"),
		"selected", 0,
		N_("Main menu selected hotkey colors.")),

	INIT_OPT_COLOR("ui.colors.mono.mainmenu.hotkey.normal", N_("Text color"),
		"text", 0, "black",
		N_("Main menu unselected hotkey default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.mainmenu.hotkey.normal", N_("Background color"),
		"background", 0, "white",
		N_("Main menu unselected hotkey default background color.")),

	INIT_OPT_COLOR("ui.colors.mono.mainmenu.hotkey.selected", N_("Text color"),
		"text", 0, "black",
		N_("Main menu selected hotkey default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.mainmenu.hotkey.selected", N_("Background color"),
		"background", 0, "white",
		N_("Main menu selected hotkey default background color.")),


	INIT_OPT_TREE("ui.colors.mono", N_("Menu bar"),
		"menu", 0,
		N_("Menu bar colors.")),

	INIT_OPT_TREE("ui.colors.mono.menu", N_("Unselected menu item"),
		"normal", 0,
		N_("Unselected menu item colors.")),

	INIT_OPT_COLOR("ui.colors.mono.menu.normal", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.menu.normal", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.mono.menu", N_("Selected menu item"),
		"selected", 0,
		N_("Selected menu item colors.")),

	INIT_OPT_COLOR("ui.colors.mono.menu.selected", N_("Text color"),
		"text", 0, "gray",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.menu.selected", N_("Background color"),
		"background", 0, "black",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.mono.menu", N_("Marked menu item"),
		"marked", 0,
		N_("Marked menu item colors.")),

	INIT_OPT_COLOR("ui.colors.mono.menu.marked", N_("Text color"),
		"text", 0, "gray",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.menu.marked", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.mono.menu", N_("Hotkey"),
		"hotkey", 0,
		N_("Menu item hotkey colors.")),

	INIT_OPT_TREE("ui.colors.mono.menu.hotkey", N_("Unselected hotkey"),
		"normal", 0,
		N_("Menu unselected hotkey colors.")),

	INIT_OPT_TREE("ui.colors.mono.menu.hotkey", N_("Selected hotkey"),
		"selected", 0,
		N_("Menu selected hotkey colors.")),

	INIT_OPT_COLOR("ui.colors.mono.menu.hotkey.normal", N_("Text color"),
		"text", 0, "gray",
		N_("Menu unselected hotkey default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.menu.hotkey.normal", N_("Background color"),
		"background", 0, "black",
		N_("Menu unselected hotkey default background color.")),

	INIT_OPT_COLOR("ui.colors.mono.menu.hotkey.selected", N_("Text color"),
		"text", 0, "gray",
		N_("Menu selected hotkey default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.menu.hotkey.selected", N_("Background color"),
		"background", 0, "black",
		N_("Menu selected hotkey default background color.")),

	INIT_OPT_TREE("ui.colors.mono.menu", N_("Menu frame"),
		"frame", 0,
		N_("Menu frame colors.")),

	INIT_OPT_COLOR("ui.colors.mono.menu.frame", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.menu.frame", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),


	INIT_OPT_TREE("ui.colors.mono", N_("Dialog"),
		"dialog", 0,
		N_("Dialog colors.")),

	/* Compatibility alias: added by jonas at 2003-10-18, 0.5pre7.CVS. */
	INIT_OPT_ALIAS("ui.colors.mono.dialog", NULL,
		"background", 0, "ui.colors.mono.dialog.generic.background",
		N_("This option is deprecated and will be removed very soon.\n"
		"Please use ui.colors.mono.dialog.generic.background instead.")),

	INIT_OPT_TREE("ui.colors.mono.dialog", N_("Generic"),
		"generic", 0,
		N_("Generic dialog colors.")),

	INIT_OPT_COLOR("ui.colors.mono.dialog.generic", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.dialog.generic", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.mono.dialog", N_("Frame"),
		"frame", 0,
		N_("Dialog frame colors.")),

	INIT_OPT_COLOR("ui.colors.mono.dialog.frame", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.dialog.frame", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.mono.dialog", N_("Title"),
		"title", 0,
		N_("Dialog title colors.")),

	INIT_OPT_COLOR("ui.colors.mono.dialog.title", N_("Text color"),
		"text", 0, "gray",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.dialog.title", N_("Background color"),
		"background", 0, "black",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.mono.dialog", N_("Text"),
		"text", 0,
		N_("Dialog text colors.")),

	INIT_OPT_COLOR("ui.colors.mono.dialog.text", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.dialog.text", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.mono.dialog", N_("Checkbox"),
		"checkbox", 0,
		N_("Dialog checkbox colors.")),

	INIT_OPT_COLOR("ui.colors.mono.dialog.checkbox", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.dialog.checkbox", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.mono.dialog", N_("Checkbox label"),
		"checkbox-label", 0,
		N_("Dialog checkbox label colors.")),

	INIT_OPT_COLOR("ui.colors.mono.dialog.checkbox-label", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.dialog.checkbox-label", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.mono.dialog", N_("Button"),
		"button", 0,
		N_("Dialog button colors.")),

	INIT_OPT_COLOR("ui.colors.mono.dialog.button", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.dialog.button", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.mono.dialog", N_("Selected button"),
		"button-selected", 0,
		N_("Dialog selected button colors.")),

	INIT_OPT_COLOR("ui.colors.mono.dialog.button-selected", N_("Text color"),
		"text", 0, "white",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.dialog.button-selected", N_("Background color"),
		"background", 0, "black",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.mono.dialog", N_("Text field"),
		"field", 0,
		N_("Dialog field colors.")),

	INIT_OPT_COLOR("ui.colors.mono.dialog.field", N_("Text color"),
		"text", 0, "gray",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.dialog.field", N_("Background color"),
		"background", 0, "black",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.mono.dialog", N_("Text field text"),
		"field-text", 0,
		N_("Dialog field text colors.")),

	INIT_OPT_COLOR("ui.colors.mono.dialog.field-text", N_("Text color"),
		"text", 0, "gray",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.dialog.field-text", N_("Background color"),
		"background", 0, "black",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.mono.dialog", N_("Meter"),
		"meter", 0,
		N_("Dialog meter colors.")),

	INIT_OPT_COLOR("ui.colors.mono.dialog.meter", N_("Text color"),
		"text", 0, "gray",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.dialog.meter", N_("Background color"),
		"background", 0, "black",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.mono.dialog", N_("Shadow"),
		"shadow", 0,
		N_("Dialog shadow colors (see ui.shadows option).")),

	INIT_OPT_COLOR("ui.colors.mono.dialog.shadow", N_("Background color"),
		"background", 0, "black",
		N_("Default background color.")),

	INIT_OPT_COLOR("ui.colors.mono.dialog.shadow", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),


	INIT_OPT_TREE("ui.colors.mono", N_("Title bar"),
		"title", 0,
		N_("Title bar colors.")),

	INIT_OPT_TREE("ui.colors.mono.title", N_("Generic title bar"),
		"title-bar", 0,
		N_("Generic title bar colors.")),

	INIT_OPT_COLOR("ui.colors.mono.title.title-bar", N_("Text color"),
		"text", 0, "gray",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.title.title-bar", N_("Background color"),
		"background", 0, "black",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.mono.title", N_("Title text"),
		"title-text", 0,
		N_("Title bar text colors.")),

	INIT_OPT_COLOR("ui.colors.mono.title.title-text", N_("Text color"),
		"text", 0, "gray",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.title.title-text", N_("Background color"),
		"background", 0, "black",
		N_("Default background color.")),


	INIT_OPT_TREE("ui.colors.mono", N_("Status bar"),
		"status", 0,
		N_("Status bar colors.")),

	INIT_OPT_TREE("ui.colors.mono.status", N_("Generic status bar"),
		"status-bar", 0,
		N_("Generic status bar colors.")),

	INIT_OPT_COLOR("ui.colors.mono.status.status-bar", N_("Text color"),
		"text", 0, "gray",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.status.status-bar", N_("Background color"),
		"background", 0, "black",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.mono.status", N_("Status bar text"),
		"status-text", 0,
		N_("Status bar text colors.")),

	INIT_OPT_COLOR("ui.colors.mono.status.status-text", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.status.status-text", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),


	INIT_OPT_TREE("ui.colors.mono", N_("Tabs bar"),
		"tabs", 0,
		N_("Tabs bar colors.")),

	INIT_OPT_TREE("ui.colors.mono.tabs", N_("Unselected tab"),
		"normal", 0,
		N_("Unselected tab colors.")),

	INIT_OPT_COLOR("ui.colors.mono.tabs.normal", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.tabs.normal", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.mono.tabs", N_("Selected tab"),
		"selected", 0,
		N_("Selected tab colors.")),

	INIT_OPT_COLOR("ui.colors.mono.tabs.selected", N_("Text color"),
		"text", 0, "gray",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.tabs.selected", N_("Background color"),
		"background", 0, "black",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.mono.tabs", N_("Tab separator"),
		"separator", 0,
		N_("Tab separator colors.")),

	INIT_OPT_COLOR("ui.colors.mono.tabs.separator", N_("Text color"),
		"text", 0, "gray",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.tabs.separator", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),

	INIT_OPT_TREE("ui.colors.mono", N_("Searched strings"),
		"searched", 0,
		N_("Searched string highlight colors.")),

	INIT_OPT_COLOR("ui.colors.mono.searched", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_COLOR("ui.colors.mono.searched", N_("Background color"),
		"background", 0, "white",
		N_("Default background color.")),


	/* ========================================================== */
	/* ============= BORING PART (colors) END =================== */
	/* ========================================================== */

	/* Keep options in alphabetical order. */


	INIT_OPT_TREE("ui", N_("Dialog settings"),
		"dialogs", 0,
		N_("Dialogs-specific appearance and behaviour settings.")),

	INIT_OPT_INT("ui.dialogs", N_("Minimal height of listbox widget"),
		"listbox_min_height", 0, 1, 20, 10,
		N_("Minimal height of the listbox widget (used e.g. for bookmarks\n"
		"or global history).")),

	INIT_OPT_BOOL("ui.dialogs", N_("Drop shadows"),
		"shadows", 0, 0,
		N_("Make dialogs drop shadows (the shadows are solid, you can\n"
		"adjust their color by ui.colors.*.dialog.shadow). You may\n"
		"also want to eliminate the wide borders by adjusting setup.h.")),


	INIT_OPT_TREE("ui", N_("Timer options"),
		"timer", 0,
		N_("Timed action after certain interval of user inactivity. Someone can\n"
		"even find this useful, although you may not believe that.")),

#ifdef USE_LEDS
	INIT_OPT_INT("ui.timer", N_("Enable"),
		"enable", 0, 0, 2, 0,
		N_("Whether to enable the timer or not:\n"
		"0 is don't count down anything\n"
		"1 is count down, but don't show the timer\n"
		"2 is count down and show the timer near LEDs")),
#else
	INIT_OPT_INT("ui.timer", N_("Enable"),
		"enable", 0, 0, 2, 0,
		N_("Whether to enable the timer or not:\n"
		"0 is don't count down anything\n"
		"1 is count down, but don't show the timer\n"
		"2 is count down and show the timer near LEDs (DISABLED)")),
#endif

	INIT_OPT_INT("ui.timer", N_("Duration"),
		"duration", 0, 1, 86400, 86400,
		N_("Inactivity timeout. One day should be enough for just everyone (TM).")),

	INIT_OPT_STRING("ui.timer", N_("Action"),
		"action", 0, "",
		N_("Key-binding action to be triggered when timer reaches zero.")),


	INIT_OPT_TREE("ui", N_("Window tabs"),
		"tabs", 0,
		N_("Window tabs settings.")),

	INIT_OPT_INT("ui.tabs", N_("Display tabs bar"),
		"show_bar",  0, 0, 2, 1,
		N_("Show tabs bar on the screen:\n"
		   "0 means never.\n"
		   "1 means only if two or more tabs.\n"
		   "2 means always.")),

	INIT_OPT_BOOL("ui.tabs", N_("Wrap-around tabs cycling"),
		"wraparound", 0, 1,
		N_("When moving right from the last tab, jump at the first one, and\n"
		"vice versa.")),



	INIT_OPT_LANGUAGE("ui", N_("Language"),
		"language", 0,
		N_("Language of user interface. System means that the language will\n"
		"be extracted from the environment dynamically.")),

	/* Compatibility alias: added by pasky at 2002-12-01, 0.4pre20.CVS.
	 * Estimated due time: 2003-02-01 */
	INIT_OPT_ALIAS("ui", NULL,
		"shadows", 0, "ui.dialogs.shadows",
		N_("This option is deprecated and will be removed very soon.\n"
		"Please use the ui.dialog.shadows option instead.")),

	INIT_OPT_BOOL("ui", N_("Display status bar"),
		"show_status_bar", 0, 1,
		N_("Show status bar on the screen.")),

	INIT_OPT_BOOL("ui", N_("Display title bar"),
		"show_title_bar", 0, 1,
		N_("Show title bar on the screen.")),

	INIT_OPT_BOOL("ui", N_("Display goto dialog on startup"),
		"startup_goto_dialog", 0, 1,
		N_("Pop up goto dialog on startup when there's no homepage set.")),


	INIT_OPT_BOOL("ui", N_("Set window title"),
		"window_title", 0, 1,
		N_("Set the window title when running in a windowing environment\n"
		"in an xterm-like terminal. This way the document's title is\n"
		"shown on the window titlebar.")),



	/* Compatibility alias: added by pasky at 2002-12-10, 0.4pre24.CVS.
	 * Estimated due time: 2003-02-10 */
	INIT_OPT_ALIAS("", NULL,
		"config_saving_style", 0, "config.saving_style",
		N_("This option is deprecated and will be removed very soon.\n"
		"Please use the config.saving_style option instead.")),

	INIT_OPT_BOOL("", N_("Use secure file saving"),
		"secure_file_saving", 0, 1,
		N_("First write data to 'file.tmp', then rename to 'file' upon\n"
		"successfully finishing this. Note that this relates only to\n"
		"config files, not downloaded files. You may want to disable\n"
		"it, if you want some config file with some exotic permissions.\n"
		"Secure file saving is automagically disabled if file is symlink.")),

	NULL_OPTION_INFO,
};

/* Keep options in alphabetical order. */
/* Commandline options */

struct option_info cmdline_options_info[] = {
	INIT_OPT_BOOL("", N_("Restrict to anonymous mode"),
		"anonymous", 0, 0,
		N_("Restrict ELinks so that it can run on an anonymous account.\n"
		"No local file browsing, no downloads. Execution of viewers\n"
		"is allowed, but user can't add or modify entries in\n"
		"association table.")),

	INIT_OPT_BOOL("", N_("Autosubmit first form"),
		"auto-submit", 0, 0,
		N_("Go and submit the first form you'll stumble upon.")),

	INIT_OPT_INT("", N_("Clone session with given ID"),
		"base-session", 0, 0, MAXINT, 0,
		N_("ID of session (ELinks instance) which we want to clone.\n"
		"This is internal ELinks option, you don't want to use it.")),

	INIT_OPT_STRING("", N_("Set config dir to given string"),
		"confdir", 0, "",
		N_("Set the config dir to the given path. ELinks will read\n"
		"its config files and writes to it. If the path begins with\n"
		"a '/' its used as an absolute path. Else it is assumed to\n"
		"be relative to your HOME dir.")),

	INIT_OPT_ALIAS("", N_("MIME type to assume for documents"),
		"default-mime-type", 0, "mime.default_type",
		N_("Default MIME type to assume for documents of unknown type.")),

	INIT_OPT_BOOL("", N_("Write formatted version of given URL to stdout"),
		"dump", 0, 0,
		N_("Write a plain-text version of the given HTML document to\n"
		"stdout.")),

	INIT_OPT_ALIAS("", N_("Codepage to use with -dump"),
		"dump-charset", 0, "document.dump.codepage",
		N_("Codepage used in dump output.")),

	INIT_OPT_ALIAS("", N_("Width of document formatted with -dump"),
		"dump-width", 0, "document.dump.width",
		N_("Width of the dump output.")),

	INIT_OPT_COMMAND("", N_("Evaluate given configuration option"),
		"eval", 0, eval_cmd,
		N_("Specify elinks.conf config options on the command-line:\n"
		"  -eval 'set protocol.file.allow_special_files = 1'")),

	/* lynx compatibility */
	INIT_OPT_COMMAND("", N_("Assume the file is HTML"),
		"force-html", 0, forcehtml_cmd,
		N_("This makes ELinks assume that the files it sees are HTML. This is\n"
		"equivalent to -default-mime-type text/html.")),

	/* XXX: -?, -h and -help share the same caption and should be kept in
	 * the current order for usage help printing to be ok */
	INIT_OPT_COMMAND("", NULL, "?", 0, printhelp_cmd, NULL),

	INIT_OPT_COMMAND("", NULL, "h", 0, printhelp_cmd, NULL),

	INIT_OPT_COMMAND("", N_("Print usage help and exit"),
		"help", 0, printhelp_cmd,
		N_("Print usage help and exit.")),

	INIT_OPT_COMMAND("", N_("Print detailed usage help and exit"),
		"long-help", 0, printhelp_cmd,
		N_("Print detailed usage help and exit.")),

	INIT_OPT_COMMAND("", N_("Print help for configuration options"),
		"config-help", 0, printhelp_cmd,
		N_("Print help on configuration options and exit.")),

	INIT_OPT_COMMAND("", N_("Look up specified host"),
		"lookup", 0, lookup_cmd,
		N_("Look up specified host.")),

	INIT_OPT_BOOL("", N_("Run as separate instance"),
		"no-connect", 0, 0,
		N_("Run ELinks as a separate instance instead of connecting to an\n"
		"existing instance. Note that normally no runtime state files\n"
		"(bookmarks, history and so on) are written to the disk when\n"
		"this option is used. See also -touch-files.")),

	INIT_OPT_BOOL("", N_("Don't use files in ~/.elinks"),
		"no-home", 0, 0,
		N_("Don't attempt to create and/or use home rc directory (~/.elinks).")),

	INIT_OPT_INT("", N_("Connect to session ring with given ID"),
		"session-ring", 0, 0, MAXINT, 0,
		N_("ID of session ring this ELinks session should connect to. ELinks\n"
		"works in so-called session rings, whereby all instances of ELinks\n"
		"are interconnected and share state (cache, bookmarks, cookies,\n"
		"and so on). By default, all ELinks instances connect to session\n"
		"ring 0. You can change that behaviour with this switch and form as\n"
		"many session rings as you want. Obviously, if the session-ring with\n"
		"this number doesn't exist yet, it's created and this ELinks instance\n"
		"will become the master instance (that usually doesn't matter for you\n"
		"as a user much). Note that you usually don't want to use this unless\n"
		"you're a developer and you want to do some testing - if you want the\n"
		"ELinks instances each running standalone, rather use the -no-connect\n"
		"command-line option. Also note that normally no runtime state files\n"
		"are written to the disk when this option is used. See also\n"
		"-touch-files.")),

	INIT_OPT_BOOL("", N_("Write the source of given URL to stdout"),
		"source", 0, 0,
		N_("Write the given HTML document in source form to stdout.")),

	INIT_OPT_BOOL("", N_("Read document from stdin"),
		"stdin", 0, 0,
		N_("Open stdin as an HTML document - this is fully equivalent to:\n"
		" -eval 'set protocol.file.allow_special_files = 1' file:///dev/stdin\n"
		"Use whichever suits you more ;-). Note that reading document from\n"
		"stdin WORKS ONLY WHEN YOU USE -dump OR -source!! (I would like to\n"
		"know why you would use -source -stdin, though ;-)")),

	INIT_OPT_BOOL("", N_("Touch files in ~/.elinks when running with -no-connect/-session-ring"),
		"touch-files", 0, 0,
		N_("Set to 1 to have runtime state files (bookmarks, history, ...)\n"
		"changed even when -no-connect or -session-ring is used; has no\n"
		"effect if not used in connection with any of these options.")),

	INIT_OPT_COMMAND("", N_("Print version information and exit"),
		"version", 0, version_cmd,
		N_("Print ELinks version information and exit.")),

	NULL_OPTION_INFO,
};
