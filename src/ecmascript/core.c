/* ECMAScript engine core */
/* $Id: */

#include <js.h>

#define NORMAL JS_PROP_EXPORTED | JS_PROP_PERMANENT

static struct JSPropertySpec DOMImplementation_props[] = {
{0}};

static struct JSFunctionSpec DOMImplementation_methods[] = {
{"hasFeature", DOMImplementation_hasFeature, 2, 0, 0},
{"createDocumentType", DOMIplementation_createDocumentType, 3, 0, 0},
{"createDocument", DOMImplemenation_createDocument, 3, 0, 0},
{0}};

static struct JSPropertySpec Node_props[] = {
{"nodeName", 13, NORMAL | JS_PROP_READONLY, Node_get_nodeName, NULL},
{"nodeValue", 14, NORMAL, Node_get_nodeValue, Node_set_nodeValue},
{"nodeType", 15, NORMAL | JS_PROP_READONLY, Node_get_nodeType, NULL},
{"parentNode", 16, NORMAL | JS_PROP_READONLY, Node_get_parentNode, NULL},
{"childNodes", 17, NORMAL | JS_PROP_READONLY, Node_get_childNodes, NULL},
{"firstChild", 18, NORMAL | JS_PROP_READONLY, Node_get_firstChild, NULL},
{"lastChild", 19, NORMAL | JS_PROP_READONLY, Node_get_lastChild, NULL},
{"previousSibling", 20, NORMAL | JS_PROP_READONLY, Node_get_previousSibling, NULL},
{"nextSibling", 21, NORMAL | JS_PROP_READONLY, Node_get_nextSibling, NULL},
{"attributes", 22, NORMAL | JS_PROP_READONLY, Node_get_attributes, NULL},
{"ownerDocument", 23, NORMAL | JS_PROP_READONLY, Node_get_ownerDocument, NULL},
{"namespaceURI", 24, NORMAL | JS_PROP_READONLY, Node_get_namespaceURI, NULL},
{"prefix", 25, NORMAL , Node_get_prefix, Node_set_prefix},
{"localName", 26, NORMAL | JS_PROP_READONLY, Node_get_localName, NULL},
{0}};

static struct JSFunctionSpec Node_methods[] = {
{"insertBefore", Node_insertBefore, 2, 0, 0},
{"replaceChild", Node_replaceChild, 2, 0, 0},
{"removeChild", Node_removeChild, 1, 0, 0},
{"appendChild", Node_appendChild, 1, 0, 0},
{"hasChildNodes", Node_hasChildNodes, 0, 0, 0},
{"cloneNode", Node_cloneNode, 1, 0, 0},
{"normalize", Node_normalize, 0, 0, 0},
{"isSupported", Node_isSupported, 2, 0, 0},
{"hasAttributes", Node_hasAttributes, 0, 0, 0},
{0}};

static struct JSPropertySpec NodeList_props[] = {
{"length", 0, NORMAL | JS_PROP_READONLY, NodeList_get_length, NULL},
{0}};

static JSFunctionSpec NodeList_methods[] = {
{"item", NodeList_item, 1, 0, 0},
{0}};

static JSPropertySpec NamedNodeMap_props[] = {
{"length", 0, NORMAL | JS_PROP_READONLY, NamedNodeMap_get_length, NULL},
{0}};

static JSFunctionSpec NamedNodeMap_methods[] = {
{"getNamedItem", NamedNodeMap_getNamedItem, 1, 0, 0},
{"setNamedItem", NamedNodeMap_setNamedItem, 1, 0, 0},
{"removeNamedItem", NamedNodeMap_removeNamedItem, 1, 0, 0},
{"item", NamedNodeMap_item, 1, 0, 0},
{"getNamedItemNS", NameNodeMap_getNamedItemNS, 2, 0, 0},
{"setNamedItemNS", NameNodeMap_setNamedItemNS, 1, 0, 0},
{"removeNamedItemNS", NameNodeMap_removeNamedItemNS, 2, 0, 0},
{0}};

static JSPropertySpec CharacterData_props[] = {
{"data", 0, NORMAL, CharacterData_get_data, CharacterData_set_data},
{"length", 1, NORMAL | JS_PROP_READONLY, CharacterData_get_length, NULL},
{0}};

static JSFunctionSpec CharacterData_methods[] = {
{"substringData", CharacterData_substringData, 2, 0, 0},
{"appendData", CharacterData_appendData, 1, 0, 0},
{"insertData", CharacterData_insertData, 2, 0, 0},
{"deleteData", CharacterData_deleteData, 2, 0, 0},
{"replaceData", CharacterData_replaceData, 3, 0, 0},
{0}};

static JSPropertySpec Attr_props[] = {
{"name", 0, NORMAL | JS_PROP_READONLY, Attr_get_name, NULL},
{"specified", 1, NORMAL | JS_PROP_READONLY, Attr_get_specified, NULL},
{"value", 2, NORMAL, Attr_get_value, Attr_set_value},
{"ownerElement", 3, NORMAL | JS_PROP_READONLY, Attr_get_ownerElement, NULL},
{0}};

static JSFunctionSpec Attr_methods[] = {
{0}};

static JSPropertySpec Element_props[] = {
{"tagName", 0, NORMAL | JS_PROP_READONLY, Element_get_tagName, NULL},
{0}};

static JSFunctionSpec Element_methods[] = {
{"getAttribute", Element_getAttribute, 1, 0, 0},
{"setAttribute", Element_setAttribute, 2, 0, 0},
{"removeAttribute", Element_removeAttribute, 2, 0, 0},
{"getAttributeNode", Element_getAttributeNode, 1, 0, 0},
{"setAttributeNode", Element_setAttributeNode, 1, 0, 0},
{"removeAttributeNode", Element_removeAttributeNode, 1, 0, 0},
{"getElementsByTagName", Element_getElementsByTagName, 1, 0, 0},
{"getAttributeNS", Element_getAttributeNS, 2, 0, 0},
{"setAttributeNS", Element_setAttributeNS, 3, 0, 0},
{"removeAttributeNS", Element_removeAttributeNS, 2, 0, 0},
{"getAttributeNodeNS", Element_getAttributeNodeNS, 2, 0, 0},
{"setAttributeNodeNS", Element_setAttributeNodeNS, 1, 0, 0},
{"getElementsByTagNameNS", Element_getElementsByTagNameNS, 2, 0, 0},
{"hasAttribute", Element_hasAttribute, 1, 0, 0},
{"hasAttributeNS", Element_hasAttributeNS, 2, 0, 0},
{0}};

static JSPropertySpec Text_props[] = {
{0}};

static JSFunctionSpec Text_methods[] = {
{"splitText", Text_splitText, 1, 0, 0},
{0}};

static JSPropertySpec Comment_props[] = {
{0}};

static JSFunctionSpec Comment_methods[] = {
{0}};

static JSPropertySpec CDATASection_props[] = {
{0}};

static JSFunctionSpec CDATASection_methods[] = {
{0}};

static JSFunctionSpec DocumentType_props[] = {
{"name", 0, NORMAL | JS_PROP_READONLY, DocumentType_get_name, NULL},
{"entities", 1, NORMAL | JS_PROP_READONLY, DocumentType_get_entities, NULL},
{"notations", 2, NORMAL | JS_PROP_READONLY, DocumentType_get_notations, NULL},
{"publicId", 3, NORMAL | JS_PROP_READONLY, DocumentType_get_publicId, NULL},
{"systemId", 4, NORMAL | JS_PROP_READONLY, DocumentType_get_systemId, NULL},
{"internalSubset", 0, NORMAL | JS_PROP_READONLY, DocumentType_get_internalSubset, NULL},
{0}};

static JSFunctionSpec DocumentType_methods[] = {
{0}};

static JSFunctionSpec Notation_props[] = {
{"publicId", 0, NORMAL | JS_PROP_READONLY, Notation_get_publicId, NULL},
{"systemId", 1, NORMAL | JS_PROP_READONLY, Notation_get_systemId, NULL},
{0}};

static JSFunctionSpec Notation_methods[] = {
{0}};

static JSFunctionSpec Entity_props[] = {
{"publicId", 0, NORMAL | JS_PROP_READONLY, Entity_get_publicId, NULL},
{"systemId", 1, NORMAL | JS_PROP_READONLY, Entity_get_systemId, NULL},
{"notationName", 2, NORMAL | JS_PROP_READONLY, Entity_get_notationName, NULL},
{0}};

static JSFunctionSpec Entity_methods[] = {
{0}};

static JSPropertySpec EntityReference_props[] = {
{0}};

static JSFunctionSpec EntityReference_methods[] = {
{0}};

static JSPropertySpec ProcessingInstruction_props[] = {
{"target", 0, NORMAL | JS_PROP_READONLY, ProcessingInstruction_get_target, NULL},
{"data", 1, NORMAL, ProcessingInstruction_get_data, ProcessingInstruction_set_data},
{0}};

static JSFunctionSpec ProcessingInstruction_methods[] = {
{0}};

static JSPropertySpec DocumentFragment_props[] = {
{0}};

static JSFunctionSpec DocumentFragment_methods[] = {
{0}};

static JSPropertySpec Document_props[] = {
{"doctype", 0, NORMAL | JS_PROP_READONLY, Document_get_doctype, NULL},
{"implemantation", 1, NORMAL | JS_PROP_READONLY, Document_get_implementation, NULL},
{"documentElement", 2, NORMAL | JS_PROP_READONLY, Document_get_documentElement, NULL},
{0}};

static JSFunctionSpec Document_methods[] = {
{"createElement", Document_createElement, 1, 0, 0},
{"createDocumentFragment", Document_createDocumentFragment, 0, 0, 0},
{"createTextNode", Document_createTextNode, 1, 0, 0},
{"createComment", Document_createComment, 1, 0, 0},
{"createCDATASection", Document_createCDATASection, 1, 0, 0},
{"createProcessingInstruction", Document_createProcessingInstruction, 2, 0, 0},
{"createAttribute", Document_createAttribute, 1, 0, 0},
{"createEntityReference", Document_createEntityReference, 1, 0, 0},
{"getElementsByTagName", Document_getElementsByTagName, 1, 0, 0},
{"importNode", Document_importNode, 2, 0, 0},
{"createElementNS", Document_createElementNS, 2, 0, 0},
{"createAttributeNS", Document_createAttributeNS, 2, 0, 0},
{"getElementsByTagNameNS", Document_getElementsByTagNameNS, 2, 0, 0},
{"getElementById", Document_getElementByTagId, 1, 0, 0},
{0}};

static JSPropertySpec HTMLCollection_props[] = {
{"length", 0, NORMAL | JS_PROP_READONLY, HTMLCollection_get_length, NULL},
{0}};

static JSFunctionSpec HTMLCollection_methods[] = {
{"item", HTMLCollection_item, 1, 0, 0},
{"namedItem", HTMLCollection_namedItem, 1, 0, 0},
{0}};

static JSPropertySpec HTMLOptionsCollection_props[] = {
{"length", 0, NORMAL, HTMLOptionsCollection_get_length, HTMLOptionsCollection_set_length},
{0}};

static JSFunctionSpec HTMLOptionsCollection_methods[] = {
{"item", HTMLOptionsCollection_item, 1, 0, 0},
{"namedItem", HTMLOptionsCollection_namedItem, 1, 0, 0},
{0}};

static JSPropertySpec HTMLDocument_props[] = {
{"title", 0, NORMAL, HTMLDocument_get_title, HTMLDocument_set_title},
{"referer", 1, NORMAL | JS_PROP_READONLY, HTMLDocument_get_referer, NULL},
{"domain", 2, NORMAL | JS_PROP_READONLY, HTMLDocument_get_domain, NULL},
{"URL", 3, NORMAL | JS_PROP_READONLY, HTMLDocument_get_URL, NULL},
{"body", 4, NORMAL, HTMLDocument_get_body, HTMLDocument_set_body},
{"images", 5, NORMAL | JS_PROP_READONLY, HTMLDocument_get_images, NULL},
{"applets", 6, NORMAL | JS_PROP_READONLY, HTMLDocument_get_applets, NULL},
{"links", 7, NORMAL | JS_PROP_READONLY, HTMLDocument_get_links, NULL},
{"forms", 8, NORMAL | JS_PROP_READONLY, HTMLDocument_get_forms, NULL},
{"anchors", 9, NORMAL | JS_PROP_READONLY, HTMLDocument_get_anchors, NULL},
{"cookie", 10, NORMAL, HTMLDocument_get_cookie, HTMLDocument_set_cookie},
{0}};

static JSFunctionSpec HTMLDocument_methods[] = {
{"open", HTMLDocument_open, 0, 0, 0},
{"close", HTMLDocument_close, 0, 0, 0},
{"write", HTMLDocument_write, 1, 0, 0},
{"writeln", HTMLDocument_writeln, 1, 0, 0},
{"getElementsByName", HTMLDocument_getElementsByName, 1, 0, 0},
{0}};

static JSPropertySpec HTMLElement_props[] = {
{"id", 0, NORMAL, HTMLElement_get_id, HTMLElement_set_id},
{"title", 1, NORMAL, HTMLElement_get_title, HTMLElement_set_title},
{"lang", 2, NORMAL, HTMLElement_get_lang, HTMLElement_set_lang},
{"dir", 3, NORMAL, HTMLElement_get_dir, HTMLElement_set_dir},
{"className", 4, NORMAL, HTMLElement_get_className, HTMLElement_set_className},
{0}};

static JSFunctionSpec HTMLElement_methods[] = {
{0}};

static JSPropertySpec HTMLHtmlElement_props[] = {
{"version", 0, NORMAL, HTMLHtmlElement_get_version, HTMLHtmlElement_set_version},
{0}};

static JSFunctionSpec HTMLHtmlElement_methods[] = {
{0}};

static JSPropertySpec HTMLHeadElement_props[] = {
{"profile", 0, NORMAL, HTMLHeadElement_get_profile, HTMLHeadElement_set_profile},
{0}};

static JSFunctionSpec HTMLHeadElement_methods[] = {
{0}};

static JSPropertySpec HTMLLinkElement_props[] = {
{"disabled", 0, NORMAL, HTMLLinkElement_get_disabled, HTMLLinkElement_set_disabled},
{"charset", 1, NORMAL, HTMLLinkElement_get_charset, HTMLLinkElement_set_charset},
{"href", 2, NORMAL, HTMLLinkElement_get_href, HTMLLinkElement_set_href},
{"hreflang", 3, NORMAL, HTMLLinkElement_get_hreflang, HTMLLinkElement_set_hreflang},
{"media", 4, NORMAL, HTMLLinkElement_get_media, HTMLLinkElement_set_media},
{"rel", 5, NORMAL, HTMLLinkElement_get_rel, HTMLLinkElement_set_rel},
{"rev", 6, NORMAL, HTMLLinkElement_get_rev, HTMLLinkElement_set_rev},
{"target", 7, NORMAL, HTMLLinkElement_get_target, HTMLLinkElement_set_target},
{"target", 8, NORMAL, HTMLLinkElement_get_type, HTMLLinkElement_set_type},
{0}};

static JSFunctionSpec HTMLLinkElement_methods[] = {
{0}};

static JSPropertySpec HTMLTitleElement_props[] = {
{"text", 0, NORMAL, HTMLTitleElement_get_text, HTMLTitleElement_set_text},
{0}};

static JSFunctionSpec HTMLTitleElement_methods[] = {
{0}};

static JSPropertySpec HTMLMetaElement_props[] = {
{"content", 0, NORMAL, HTMLMetaElement_get_content, HTMLMetaElement_set_content},
{"httpEquiv", 1, NORMAL, HTMLMetaElement_get_httpEquiv, HTMLMetaElement_set_httpEquiv},
{"name", 2, NORMAL, HTMLMetaElement_get_name, HTMLMetaElement_set_name},
{"scheme", 3, NORMAL, HTMLMetaElement_get_scheme, HTMLMetaElement_set_scheme},
{0}};

static JSFunctionSpec HTMLMetaElement_methods[] = {
{0}};

static JSPropertySpec HTMLBaseElement_props[] = {
{"href", 0, NORMAL, HTMLBaseElement_get_href, HTMLBaseElement_set_href},
{"target", 1, NORMAL, HTMLBaseElement_get_target, HTMLBaseElement_set_target},
{0}};

static JSFunctionSpec HTMLBaseElement_methods[] = {
{0}};

static JSPropertySpec HTMLIsIndexElement_props[] = {
{"form", 0, NORMAL | JS_PROP_READONLY, HTMLIsIndexElement_get_form, NULL},
{"prompt", 1, NORMAL, HTMLIsIndexElement_get_prompt, HTMLIsIndexElement_set_prompt},
{0}};

static JSFunctionSpec HTMLIsIndexElement_methods[] = {
{0}};

static JSPropertySpec HTMLStyleElement_props[] = {
{"disabled", 0, NORMAL, HTMLStyleElement_get_disabled, HTMLStyleElement_set_disabled},
{"media", 1, NORMAL, HTMLStyleElement_get_media, HTMLStyleElement_set_media},
{"type", 2, NORMAL, HTMLStyleElement_get_type, HTMLStyleElement_set_type},
{0}};

static JSFunctionSpec HTMLStyleElement_methods[] = {
{0}};

static JSPropertySpec HTMLBodyElement_props[] = {
{"aLink", 0, NORMAL, HTMLBodyElement_get_aLink, HTMLBodyElement_set_aLink},
{"background", 1, NORMAL, HTMLBodyElement_get_background, HTMLBodyElement_set_background},
{"bgColor", 2, NORMAL, HTMLBodyElement_get_bgColor, HTMLBodyElement_set_bgColor},
{"link", 3, NORMAL, HTMLBodyElement_get_link, HTMLBodyElement_set_link},
{"text", 4, NORMAL, HTMLBodyElement_get_text, HTMLBodyElement_set_text},
{"vLink", 5, NORMAL, HTMLBodyElement_get_vLink, HTMLBodyElement_set_vLink},
{0}};

static JSFunctionSpec HTMLBodyElement_methods[] = {
{0}};

static JSPropertySpec HTMLFormElement_props[] = {
{"elements", 0, NORMAL | JS_PROP_READONLY, HTMLFormElement_get_elements, NULL},
{"length", 1, NORMAL | JS_PROP_READONLY, HTMLFormElement_get_length, NULL},
{"name", 2, NORMAL, HTMLFormElement_get_name, HTMLFormElement_set_name},
{"acceptCharset", 3, NORMAL, HTMLFormElement_get_acceptCharset, HTMLFormElement_set_acceptCharset},
{"action", 4, NORMAL, HTMLFormElement_get_action, HTMLFormElement_set_action},
{"enctype", 5, NORMAL, HTMLFormElement_get_enctype, HTMLFormElement_set_enctype},
{"method", 6, NORMAL, HTMLFormElement_get_method, HTMLFormElement_set_method},
{"target", 7, NORMAL, HTMLFormElement_get_target, HTMLFormElement_set_target},
{0}};

static JSFunctionSpec HTMLFormElement_methods[] = {
{"submit", HTMLFormElement_submit, 0, 0, 0},
{"reset", HTMLFormElement_reset, 0, 0, 0},
{0}};

static JSPropertySpec HTMLSelectElement_props[] = {
{"type", 0, NORMAL | JS_PROP_READONLY, HTMLFormElement_get_type, NULL},
{"selectedIndex", 1, NORMAL, HTMLFormElement_get_selectedIndex, HTMLFormElement_set_selectedIndex},
{"value", 2, NORMAL, HTMLFormElement_get_value, HTMLFormElement_set_value},
{"length", 3, NORMAL, HTMLFormElement_get_length, HTMLFormElement_set_length},
{"form", 4, NORMAL | JS_PROP_READONLY, HTMLFormElement_get_form, NULL},
{"options", 5, NORMAL | JS_PROP_READONLY, HTMLFormElement_get_options, NULL},
{"disabled", 6, NORMAL, HTMLFormElement_get_disabled, HTMLFormElement_set_disabled},
{"multiple", 7, NORMAL, HTMLFormElement_get_multiple, HTMLFormElement_set_multiple},
{"name", 8, NORMAL, HTMLFormElement_get_name, HTMLFormElement_set_name},
{"size", 9, NORMAL, HTMLFormElement_get_size, HTMLFormElement_set_size},
{"tabIndex", 10, NORMAL, HTMLFormElement_get_tabIndex, HTMLFormElement_set_tabIndex},
{0}};

static JSFunctionSpec HTMLSelectElement_methods[] = {
{"add", HTMLSelectElement_add, 2, 0, 0},
{"remove", HTMLSelectElement_remove, 1, 0, 0},
{"blur", HTMLSelectElement_blur, 0, 0, 0},
{"focus", HTMLSelectElement_focus, 0, 0, 0},
{0}};

static JSPropertySpec HTMLOptGroupElement_props[] = {
{"disabled", 0, NORMAL, HTMLOptGroupElement_get_disabled, HTMLOptGroupElement_set_disabled},
{"label", 1, NORMAL, HTMLOptGroupElement_get_label, HTMLOptGroupElement_set_label},
{0}};

static JSFunctionSpec HTMLOptGroupElement_methods[] = {
{0}};

static JSPropertySpec HTMLOptionElement_props[] = {
{"form", 0, NORMAL | JS_PROP_READONLY, HTMLOptionElement_get_form, NULL},
{"defaultSelected", 1, NORMAL, HTMLOptionElement_get_defaultSelected, HTMLOptionElement_set_defaultSelected},
{"text", 2, NORMAL | JS_PROP_READONLY, HTMLOptionElement_get_text, NULL},
{"index", 3, NORMAL | JS_PROP_READONLY, HTMLOptionElement_get_index, NULL},
{"disabled", 4, NORMAL, HTMLOptionElement_get_disabled, HTMLOptionElement_set_disabled},
{"label", 5, NORMAL, HTMLOptionElement_get_label, HTMLOptionElement_set_label},
{"selected", 6, NORMAL, HTMLOptionElement_get_selected, HTMLOptionElement_set_selected},
{"value", 7, NORMAL, HTMLOptionElement_get_value, HTMLOptionElement_set_value},
{0}};

static JSFunctionSpec HTMLOptionElement_methods[] = {
{0}};

static JSPropertySpec HTMLInputElement_props[] = {
{"defaultValue", 0, NORMAL, HTMLInputElement_get_defaultValue, HTMLInputElement_set_defaultValue},
{"defaultChecked", 1, NORMAL, HTMLInputElement_get_defaultChecked, HTMLInputElement_set_defaultChecked},
{"form", 2, NORMAL | JS_PROP_READONLY, HTMLInputElement_get_form, NULL},
{"accept", 3, NORMAL, HTMLInputElement_get_accept, HTMLInputElement_set_accept},
{"accessKey", 5, NORMAL, HTMLInputElement_get_accessKey, HTMLInputElement_set_accessKey},
{"alt", 6, NORMAL, HTMLInputElement_get_alt, HTMLInputElement_set_alt},
{"checked", 7, NORMAL, HTMLInputElement_get_checked, HTMLInputElement_set_checked},
{"disabled", 8, NORMAL, HTMLInputElement_get_disabled, HTMLInputElement_set_disabled},
{"maxLength", 9, NORMAL, HTMLInputElement_get_maxLength, HTMLInputElement_set_maxLength},
{"name", 10, NORMAL, HTMLInputElement_get_name, HTMLInputElement_set_name},
{"readOnly", 11, NORMAL, HTMLInputElement_get_readOnly, HTMLInputElement_set_readOnly},
{"size", 12, NORMAL, HTMLInputElement_get_size, HTMLInputElement_set_size},
{"src", 13, NORMAL, HTMLInputElement_get_src, HTMLInputElement_set_src},
{"tabIndex", 14, NORMAL, HTMLInputElement_get_tabIndex, HTMLInputElement_set_tabIndex},
{"type", 15, NORMAL, HTMLInputElement_get_type, HTMLInputElement_set_type},
{"useMap", 16, NORMAL, HTMLInputElement_get_useMap, HTMLInputElement_set_useMap},
{"value", 17, NORMAL, HTMLInputElement_get_value, HTMLInputElement_set_value},
{0}};

static JSFunctionSpec HTMLInputElement_methods[] = {
{"blur", HTMLInputElement_blur, 0, 0, 0},
{"focus", HTMLInputElement_focus, 0, 0, 0},
{"select", HTMLInputElement_select, 0, 0, 0},
{"click", HTMLInputElement_click, 0, 0, 0},
{0}};

static JSPropertySpec HTMLTextAreaElement_props[] = {
{"defaultValue", 0, NORMAL, HTMLTextAreaElement_get_defaultValue, HTMLTextAreaElement_set_defaultValue},
{"form", 1, NORMAL | JS_PROP_READONLY, HTMLTextAreaElement_get_form, NULL},
{"accessKey", 2, NORMAL, HTMLTextAreaElement_get_accessKey, HTMLTextAreaElement_set_accessKey},
{"cols", 3, NORMAL, HTMLTextAreaElement_get_cols, HTMLTextAreaElement_set_cols},
{"disabled", 4, NORMAL, HTMLTextAreaElement_get_disabled, HTMLTextAreaElement_set_disabled},
{"name", 5, NORMAL, HTMLTextAreaElement_get_name, HTMLTextAreaElement_set_name},
{"readOnly", 6, NORMAL, HTMLTextAreaElement_get_readOnly, HTMLTextAreaElement_set_readOnly},
{"rows", 7, NORMAL, HTMLTextAreaElement_get_rows, HTMLTextAreaElement_set_rows},
{"tabIndex", 8, NORMAL, HTMLTextAreaElement_get_tabIndex, HTMLTextAreaElement_set_tabIndex},
{"type", 9, NORMAL | JS_PROP_READONLY, HTMLTextAreaElement_get_type, NULL},
{"value", 10, NORMAL, HTMLTextAreaElement_get_value, HTMLTextAreaElement_set_value},
{0}};

static JSFunctionSpec HTMLTextAreaElement_methods[] = {
{"blur", HTMLTextAreaElement_blur, 0, 0, 0},
{"focus", HTMLTextAreaElement_focus, 0, 0, 0},
{"select", HTMLTextAreaElement_select, 0, 0, 0},
{0}};

static JSPropertySpec HTMLButtonElement_props[] = {
{"form", 0, NORMAL | JS_PROP_READONLY, HTMLButtonElement_get_form, NULL},
{"accessKey", 1, NORMAL, HTMLButtonElement_get_accessKey, HTMLButtonElement_set_accessKey},
{"disabled", 2, NORMAL, HTMLButtonElement_get_disabled, HTMLButtonElement_set_disabled},
{"name", 3, NORMAL, HTMLButtonElement_get_name, HTMLButtonElement_set_name},
{"tabIndex", 4, NORMAL, HTMLButtonElement_get_tabIndex, HTMLButtonElement_set_tabIndex},
{"type", 5, NORMAL | JS_PROP_READONLY, HTMLButtonElement_get_type, NULL},
{"value", 6, NORMAL, HTMLButtonElement_get_value, HTMLButtonElement_set_value},
{0}};

static JSFunctionSpec HTMLButtonElement_methods[] = {
{0}};

static JSPropertySpec HTMLLabelElement_props[] = {
{"form", 0, NORMAL | JS_PROP_READONLY, HTMLLabel_get_form, NULL},
{"accessKey", 1, NORMAL, HTMLLabelElement_get_accessKey, HTMLLabelElement_set_accessKey},
{"htmlFor", 1, NORMAL, HTMLLabelElement_get_htmlFor, HTMLLabelElement_set_htmlFor},
{0}};

static JSFunctionSpec HTMLLabelElement_methods[] = {
{0}};

static JSPropertySpec HTMLFieldSetElement_props[] = {
{"form", 0, NORMAL | JS_PROP_READONLY, HTMLFieldSetElement_get_form, NULL},
{0}};

static JSFunctionSpec HTMLFieldSetElement_methods[] = {
{0}};

static JSPropertySpec HTMLLegendElement_props[] = {
{"form", 0, NORMAL | JS_PROP_READONLY, HTMLLegend_get_form, NULL},
{"accessKey", 1, NORMAL, HTMLLegendElement_get_accessKey, HTMLLegendElement_set_accessKey},
{"align", 2, NORMAL, HTMLLegendElement_get_align, HTMLLegendElement_set_align},
{0}};

static JSFunctionSpec HTMLLegendElement_methods[] = {
{0}};

static JSPropertySpec HTMLUListElement_props[] = {
{"compact", 0, NORMAL, HTMLUListElement_get_compact, HTMLUListElement_set_compact},
{"type", 1, NORMAL, HTMLUListElement_get_type, HTMLUListElement_set_type},
{0}};

static JSFunctionSpec HTMLUListElement_methods[] = {
{0}};

static JSPropertySpec HTMLOListElement_props[] = {
{"compact", 0, NORMAL, HTMLOListElement_get_compact, HTMLOListElement_set_compact},
{"start", 1, NORMAL, HTMLOListElement_get_start, HTMLOListElement_set_start},
{"type", 2, NORMAL, HTMLOListElement_get_type, HTMLOListElement_set_type},
{0}};

static JSFunctionSpec HTMLOListElement_methods[] = {
{0}};

static JSPropertySpec HTMLDListElement_props[] = {
{"compact", 0, NORMAL, HTMLDListElement_get_compact, HTMLDListElement_set_compact},
{0}};

static JSFunctionSpec HTMLDListElement_methods[] = {
{0}};

static JSPropertySpec HTMLDirectoryElement_props[] = {
{"compact", 0, NORMAL, HTMLDirectoryElement_get_compact, HTMLDirectoryElement_set_compact},
{0}};

static JSFunctionSpec HTMLDirectoryElement_methods[] = {
{0}};

static JSPropertySpec HTMLMenuElement_props[] = {
{"compact", 0, NORMAL, HTMLMenuElement_get_compact, HTMLMenuElement_set_compact},
{0}};

static JSFunctionSpec HTMLMenuElement_methods[] = {
{0}};

static JSPropertySpec HTMLLIElement_props[] = {
{"type", 0, NORMAL, HTMLLIElement_get_type, HTMLLIElement_set_type},
{"value", 0, NORMAL, HTMLLIElement_get_value, HTMLLIElement_set_value},
{0}};

static JSFunctionSpec HTMLLIElement_methods[] = {
{0}};

static JSPropertySpec HTMLDivElement_props[] = {
{"align", 0, NORMAL, HTMLDivElement_get_align, HTMLDivElement_set_align},
{0}};

static JSFunctionSpec HTMLDivElement_methods[] = {
{0}};

static JSPropertySpec HTMLParagraphElement_props[] = {
{"align", 0, NORMAL, HTMLParagraphElement_get_align, HTMLParagraphElement_set_align},
{0}};

static JSFunctionSpec HTMLParagraphElement_methods[] = {
{0}};

static JSPropertySpec HTMLHeadingElement_props[] = {
{"align", 0, NORMAL, HTMLHeadingElement_get_align, HTMLHeadingElement_set_align},
{0}};

static JSFunctionSpec HTMLHeadingElement_methods[] = {
{0}};

static JSPropertySpec HTMLQuoteElement_props[] = {
{"cite", 0, NORMAL, HTMLQuoteElement_get_cite, HTMLQuoteElement_set_cite},
{0}};

static JSFunctionSpec HTMLQuoteElement_methods[] = {
{0}};

static JSPropertySpec HTMLPreElement_props[] = {
{"width", 0, NORMAL, HTMLPreElement_get_width, HTMLPreElement_set_width},
{0}};

static JSFunctionSpec HTMLPreElement_methods[] = {
{0}};

static JSPropertySpec HTMLBRElement_props[] = {
{"clear", 0, NORMAL, HTMLBRElement_get_clear, HTMLBRElement_set_clear},
{0}};

static JSFunctionSpec HTMLBRElement_methods[] = {
{0}};

static JSPropertySpec HTMLBaseFontElement_props[] = {
{"color", 0, NORMAL, HTMLBaseFontElement_get_color, HTMLBaseFontElement_set_color},
{"face", 1, NORMAL, HTMLBaseFontElement_get_face, HTMLBaseFontElement_set_face},
{"size", 2, NORMAL, HTMLBaseFontElement_get_size, HTMLBaseFontElement_set_size},
{0}};

static JSFunctionSpec HTMLBaseFontElement_methods[] = {
{0}};

static JSPropertySpec HTMLFontElement_props[] = {
{"color", 0, NORMAL, HTMLFontElement_get_color, HTMLFontElement_set_color},
{"face", 1, NORMAL, HTMLFontElement_get_face, HTMLFontElement_set_face},
{"size", 2, NORMAL, HTMLFontElement_get_size, HTMLFontElement_set_size},
{0}};

static JSFunctionSpec HTMLFontElement_methods[] = {
{0}};

static JSPropertySpec HTMLHRElement_props[] = {
{"align", 0, NORMAL, HTMLHRElement_get_align, HTMLHRElement_set_align},
{"noShade", 1, NORMAL, HTMLHRElement_get_noShade, HTMLHRElement_set_noShade},
{"size", 2, NORMAL, HTMLHRElement_get_size, HTMLHRElement_set_size},
{"width", 3, NORMAL, HTMLHRElement_get_width, HTMLHRElement_set_width},
{0}};

static JSFunctionSpec HTMLHRElement_methods[] = {
{0}};

static JSPropertySpec HTMLModElement_props[] = {
{"cite", 0, NORMAL, HTMLModElement_get_cite, HTMLModElement_set_cite},
{"dateTime", 1, NORMAL, HTMLModElement_get_dateTime, HTMLModElement_set_dateTime},
{0}};

static JSFunctionSpec HTMLModElement_methods[] = {
{0}};

static JSPropertySpec HTMLAnchorElement_props[] = {
{"accessKey", 0, NORMAL, HTMLAnchorElement_get_accessKey, HTMLAnchorElement_set_accessKey},
{"charset", 1, NORMAL, HTMLAnchorElement_get_charset, HTMLAnchorElement_set_charset},
{"coords", 2, NORMAL, HTMLAnchorElement_get_coords, HTMLAnchorElement_set_coords},
{"href", 3, NORMAL, HTMLAnchorElement_get_href, HTMLAnchorElement_set_href},
{"hreflang", 4, NORMAL, HTMLAnchorElement_get_hreflang, HTMLAnchorElement_set_hreflang},
{"name", 5, NORMAL, HTMLAnchorElement_get_name, HTMLAnchorElement_set_name},
{"rel", 6, NORMAL, HTMLAnchorElement_get_rel, HTMLAnchorElement_set_rel},
{"rev", 7, NORMAL, HTMLAnchorElement_get_rev, HTMLAnchorElement_set_rev},
{"shape", 8, NORMAL, HTMLAnchorElement_get_shape, HTMLAnchorElement_set_shape},
{"tabIndex", 9, NORMAL, HTMLAnchorElement_get_tabIndex, HTMLAnchorElement_set_tabIndex},
{"target", 10, NORMAL, HTMLAnchorElement_get_target, HTMLAnchorElement_set_target},
{"type", 11, NORMAL, HTMLAnchorElement_get_type, HTMLAnchorElement_set_type},
{0}};

static JSFunctionSpec HTMLAnchorElement_methods[] = {
{"blur", HTMLAnchorElement_blur, 0, 0, 0},
{"focus", HTMLAnchorElement_focus, 0, 0, 0},
{0}};

static JSPropertySpec HTMLImageElement_props[] = {
{"name", 0, NORMAL, HTMLImageElement_get_name, HTMLImageElement_set_name},
{"align", 1, NORMAL, HTMLImageElement_get_align, HTMLImageElement_set_align},
{"alt", 2, NORMAL, HTMLImageElement_get_alt, HTMLImageElement_set_alt},
{"border", 3, NORMAL, HTMLImageElement_get_border, HTMLImageElement_set_border},
{"height", 4, NORMAL, HTMLImageElement_get_height, HTMLImageElement_set_height},
{"hspace", 5, NORMAL, HTMLImageElement_get_hspace, HTMLImageElement_set_hspace},
{"isMap", 6, NORMAL, HTMLImageElement_get_isMap, HTMLImageElement_set_isMap},
{"longDesc", 7, NORMAL, HTMLImageElement_get_longDesc, HTMLImageElement_set_longDesc},
{"src", 8, NORMAL, HTMLImageElement_get_src, HTMLImageElement_set_src},
{"useMap", 9, NORMAL, HTMLImageElement_get_useMap, HTMLImageElement_set_useMap},
{"vspace", 10, NORMAL, HTMLImageElement_get_vspace, HTMLImageElement_set_vspace},
{"width", 11, NORMAL, HTMLImageElement_get_width, HTMLImageElement_set_width},
{0}};

static JSFunctionSpec HTMLImageElement_methods[] = {
{0}};

static JSPropertySpec HTMLObjectElement_props[] = {
{"form", 0, NORMAL | JS_PROP_READONLY, HTMLObjectElement_get_form, NULL},
{"code", 1, NORMAL, HTMLObjectElement_get_code, HTMLObjectElement_set_code},
{"align", 2, NORMAL, HTMLObjectElement_get_align, HTMLObjectElement_set_align},
{"archive", 3, NORMAL, HTMLObjectElement_get_archive, HTMLObjectElement_set_archive},
{"border", 4, NORMAL, HTMLObjectElement_get_border, HTMLObjectElement_set_border},
{"codeBase", 5, NORMAL, HTMLObjectElement_get_codeBase, HTMLObjectElement_set_codeBase},
{"codeType", 6, NORMAL, HTMLObjectElement_get_codeType, HTMLObjectElement_set_codeType},
{"data", 7, NORMAL, HTMLObjectElement_get_data, HTMLObjectElement_set_data},
{"declare", 8, NORMAL, HTMLObjectElement_get_declare, HTMLObjectElement_set_declare},
{"height", 9, NORMAL, HTMLObjectElement_get_height, HTMLObjectElement_set_height},
{"hspace", 10, NORMAL, HTMLObjectElement_get_hspace, HTMLObjectElement_set_hspace},
{"name", 11, NORMAL, HTMLObjectElement_get_name, HTMLObjectElement_set_name},
{"standby", 12, NORMAL, HTMLObjectElement_get_standby, HTMLObjectElement_set_standby},
{"tabIndex", 13, NORMAL, HTMLObjectElement_get_tabIndex, HTMLObjectElement_set_tabIndex},
{"type", 14, NORMAL, HTMLObjectElement_get_type, HTMLObjectElement_set_type},
{"useMap", 15, NORMAL, HTMLObjectElement_get_useMap, HTMLObjectElement_set_useMap},
{"vspace", 16, NORMAL, HTMLObjectElement_get_vspace, HTMLObjectElement_set_vspace},
{"width", 17, NORMAL, HTMLObjectElement_get_width, HTMLObjectElement_set_width},
{"contentDocument", 18, NORMAL | JS_PROP_READONLY, HTMLObjectElement_get_contentDocument, NULL},
{0}};

static JSFunctionSpec HTMLObjectElement_methods[] = {
{0}};

static JSPropertySpec HTMLParamElement_props[] = {
{"name", 0, NORMAL, HTMLParamElement_get_name, HTMLParamElement_set_name},
{"type", 1, NORMAL, HTMLParamElement_get_type, HTMLParamElement_set_type},
{"value", 2, NORMAL, HTMLParamElement_get_value, HTMLParamElement_set_value},
{"valueType", 3, NORMAL, HTMLParamElement_get_valueType, HTMLParamElement_set_valueType},
{0}};

static JSFunctionSpec HTMLParamElement_methods[] = {
{0}};

static JSPropertySpec HTMLAppletElement_props[] = {
{"align", 0, NORMAL, HTMLAppletElement_get_align, HTMLAppletElement_set_allign},
{"alt", 1, NORMAL, HTMLAppletElement_get_alt, HTMLAppletElement_set_alt},
{"archive", 2, NORMAL, HTMLAppletElement_get_archive, HTMLAppletElement_set_archive},
{"code", 3, NORMAL, HTMLAppletElement_get_code, HTMLAppletElement_set_code},
{"codeBase", 4, NORMAL, HTMLAppletElement_get_codeBase, HTMLAppletElement_set_codeBase},
{"height", 5, NORMAL, HTMLAppletElement_get_height, HTMLAppletElement_set_height},
{"hspace", 6, NORMAL, HTMLAppletElement_get_hspace, HTMLAppletElement_set_hspace},
{"name", 7, NORMAL, HTMLAppletElement_get_name, HTMLAppletElement_set_name},
{"object", 8, NORMAL, HTMLAppletElement_get_object, HTMLAppletElement_set_object},
{"vspace", 9, NORMAL, HTMLAppletElement_get_vspace, HTMLAppletElement_set_vspace},
{"width", 10, NORMAL, HTMLAppletElement_get_width, HTMLAppletElement_set_width},
{0}};

static JSFunctionSpec HTMLAppletElement_methods[] = {
{0}};

static JSPropertySpec HTMLMapElement_props[] = {
{"areas", 0, NORMAL | JS_PROP_READONLY, HTMLMapElement_get_areas, NULL},
{"name", 1, NORMAL, HTMLMapElement_get_name, HTMLMapElement_set_name},
{0}};

static JSFunctionSpec HTMLMapElement_methods[] = {
{0}};

static JSPropertySpec HTMLAreaElement_props[] = {
{"accessKey", 0, NORMAL, HTMLAreaElement_get_accessKey, HTMLAreaElement_set_accessKey},
{"alt", 1, NORMAL, HTMLAreaElement_get_alt, HTMLAreaElement_set_alt},
{"coords", 2, NORMAL, HTMLAreaElement_get_coords, HTMLAreaElement_set_coords},
{"href", 3, NORMAL, HTMLAreaElement_get_href, HTMLAreaElement_set_href},
{"noHref", 4, NORMAL, HTMLAreaElement_get_noHref, HTMLAreaElement_set_noHref},
{"shape", 5, NORMAL, HTMLAreaElement_get_shape, HTMLAreaElement_set_shape},
{"tabIndex", 6, NORMAL, HTMLAreaElement_get_tabIndex, HTMLAreaElement_set_tabIndex},
{"target", 7, NORMAL, HTMLAreaElement_get_target, HTMLAreaElement_set_target},
{0}};

static JSFunctionSpec HTMLAreaElement_methods[] = {
{0}};

static JSPropertySpec HTMLScriptElement_props[] = {
{"text", 0, NORMAL, HTMLScriptElement_get_text, HTMLScriptElement_set_text},
{"htmlFor", 1, NORMAL, HTMLScriptElement_get_htmlFor, HTMLScriptElement_set_htmlFor},
{"event", 2, NORMAL, HTMLScriptElement_get_event, HTMLScriptElement_set_event},
{"charset", 3, NORMAL, HTMLScriptElement_get_charset, HTMLScriptElement_set_charset},
{"defer", 4, NORMAL, HTMLScriptElement_get_defer, HTMLScriptElement_set_defer},
{"src", 5, NORMAL, HTMLScriptElement_get_src, HTMLScriptElement_set_src},
{"type", 6, NORMAL, HTMLScriptElement_get_type, HTMLScriptElement_set_type},
{0}};

static JSFunctionSpec HTMLScriptElement_methods[] = {
{0}};

static JSPropertySpec HTMLTableElement_props[] = {
{"caption", 0, NORMAL, HTMLTableElement_get_caption, HTMLTableElement_set_caption},
{"tHead", 1, NORMAL, HTMLTableElement_get_tHead, HTMLTableElement_set_tHead},
{"tFoot", 2, NORMAL, HTMLTableElement_get_tFoot, HTMLTableElement_set_tFoot},
{"rows", 3, NORMAL | JS_PROP_READONLY, HTMLTableElement_get_rows, NULL},
{"tBodies", 4, NORMAL | JS_PROP_READONLY, HTMLTableElement_get_tBodies, NULL},
{"align", 5, NORMAL, HTMLTableElement_get_align, HTMLTableElement_set_align},
{"bgColor", 6, NORMAL, HTMLTableElement_get_bgColor, HTMLTableElement_set_bgColor},
{"border", 7, NORMAL, HTMLTableElement_get_border, HTMLTableElement_set_border},
{"cellSpacing", 8, NORMAL, HTMLTableElement_get_cellSpacing, HTMLTableElement_set_cellSpacing},
{"frame", 9, NORMAL, HTMLTableElement_get_frame, HTMLTableElement_set_frame},
{"rules", 10, NORMAL, HTMLTableElement_get_rules, HTMLTableElement_set_rules},
{"summary", 11, NORMAL, HTMLTableElement_get_summary, HTMLTableElement_set_summary},
{"width", 12, NORMAL, HTMLTableElement_get_width, HTMLTableElement_set_width},
{0}};

static JSFunctionSpec HTMLTableElement_methods[] = {
{"createTHead", HTMLTableElement_createTHead, 0, 0, 0},
{"deleteTHead", HTMLTableElement_deleteTHead, 0, 0, 0},
{"createTFoot", HTMLTableElement_createTFoot, 0, 0, 0},
{"deleteTFoot", HTMLTableElement_deleteTFoot, 0, 0, 0},
{"createCaption", HTMLTableElement_createCaption, 0, 0, 0},
{"deleteCaption", HTMLTableElement_deleteCaption, 0, 0, 0},
{"insertRow", HTMLTableElement_insertRow, 1, 0, 0},
{"deleteRow", HTMLTableElement_deleteRow, 1, 0, 0},
{0}};

static JSPropertySpec HTMLTableCaptionElement_props[] = {
{"align", 0, NORMAL, HTMLTableCaptionElement_get_align, HTMLTableCaptionElement_set_align},
{0}};

static JSFunctionSpec HTMLTableCaptionElement_methods[] = {
{0}};

static JSPropertySpec HTMLTableColElement_props[] = {
{"align", 0, NORMAL, HTMLTableColElement_get_align, HTMLTableColElement_set_align},
{"ch", 1, NORMAL, HTMLTableColElement_get_ch, HTMLTableColElement_set_ch},
{"chOff", 2, NORMAL, HTMLTableColElement_get_chOff, HTMLTableColElement_set_chOff},
{"span", 3, NORMAL, HTMLTableColElement_get_span, HTMLTableColElement_set_span},
{"vAlign", 4, NORMAL, HTMLTableColElement_get_vAlign, HTMLTableColElement_set_vAlign},
{"width", 5, NORMAL, HTMLTableColElement_get_width, HTMLTableColElement_set_width},
{0}};

static JSFunctionSpec HTMLTableColElement_methods[] = {
{0}};

static JSPropertySpec HTMLTableSectionElement_props[] = {
{"align", 0, NORMAL, HTMLTableSectionElement_get_align, HTMLTableSectionElement_set_align},
{"ch", 1, NORMAL, HTMLTableSectionElement_get_ch, HTMLTableSectionElement_set_ch},
{"chOff", 2, NORMAL, HTMLTableSectionElement_get_chOff, HTMLTableSectionElement_set_chOff},
{"vAlign", 3, NORMAL, HTMLTableSectionElement_get_vAlign, HTMLTableSectionElement_set_vAlign},
{"rows", 4, NORMAL | JS_PROP_READONLY, HTMLTableSectionElement_get_rows, NULL},
{0}};

static JSFunctionSpec HTMLTableSectionElement_methods[] = {
{"insertRow", HTMLTableSectionElement_insertRow, 1, 0, 0},
{"deleteRow", HTMLTebleSectionElement_deleteRow, 1, 0, 0},
{0}};

static JSPropertySpec HTMLTableRowElement_props[] = {
{"rowIndex", 0, NORMAL | JS_PROP_READONLY, HTMLTableRowElement_get_rowIndex, NULL},
{"sectionRowIndex", 1, NORMAL | JS_PROP_READONLY, HTMLTableRowElement_get_sectionRowIndex, NULL},
{"cells", 2, NORMAL | JS_PROP_READONLY, HTMLTableRowElement_get_cells, NULL},
{"align", 3, NORMAL, HTMLTableRowElement_get_align, HTMLTableRowElement_set_align},
{"bgColor", 4, NORMAL, HTMLTableRowElement_get_bgColor, HTMLTableRowElement_set_bgColor},
{"ch", 5, NORMAL, HTMLTableRowElement_get_ch, HTMLTableRowElement_set_ch},
{"chOff", 6, NORMAL, HTMLTableRowElement_get_chOff, HTMLTableRowElement_set_chOff},
{"vAlign", 7, NORMAL, HTMLTableRowElement_get_vAlign, HTMLTableRowElement_set_vAlign},
{0}};

static JSFunctionSpec HTMLTableRowElement_methods[] = {
{"insertCell", HTMLTableRowElement_insertCell, 1, 0, 0},
{"deleteCell", HTMLTebleRowElement_deleteCell, 1, 0, 0},
{0}};

static JSPropertySpec HTMLTableCellElement_props[] = {
{"cellIndex", 0, NORMAL | JS_PROP_READONLY, HTMLTableCellElement_get_cellIndex, NULL},
{"abbr", 1, NORMAL, HTMLTableCellElement_get_abbr, HTMLTableCellElement_set_abbr},
{"align", 2, NORMAL, HTMLTableCellElement_get_align, HTMLTableCellElement_set_align},
{"axis", 3, NORMAL, HTMLTableCellElement_get_axis, HTMLTableCellElement_set_axis},
{"bgColor", 4, NORMAL, HTMLTableCellElement_get_bgColor, HTMLTableCellElement_set_bgColor},
{"ch", 5, NORMAL, HTMLTableCellElement_get_ch, HTMLTableCellElement_set_ch},
{"chOff", 6, NORMAL, HTMLTableCellElement_get_chOff, HTMLTableCellElement_set_chOff},
{"colSpan", 7, NORMAL, HTMLTableCellElement_get_colSpan, HTMLTableCellElement_set_colspan},
{"headers", 8, NORMAL, HTMLTableCellElement_get_headers, HTMLTableCellElement_set_headers},
{"height", 9, NORMAL, HTMLTableCellElement_get_height, HTMLTableCellElement_set_height},
{"noWrap", 10, NORMAL, HTMLTableCellElement_get_noWrap, HTMLTableCellElement_set_noWrap},
{"rowSpan", 11, NORMAL, HTMLTableCellElement_get_rowSpan, HTMLTableCellElement_set_rowSpan},
{"scope", 12, NORMAL, HTMLTableCellElement_get_scope, HTMLTableCellElement_set_scope},
{"vAlign", 13, NORMAL, HTMLTableCellElement_get_vAlign, HTMLTableCellElement_set_vAlign},
{"width", 14, NORMAL, HTMLTableCellElement_get_width, HTMLTableCellElement_set_width},
{0}};

static JSFunctionSpec HTMLTableCellElement_methods[] = {
{0}};

static JSPropertySpec HTMLFrameSetElement_props[] = {
{"cols", 0, NORMAL, HTMLFrameSetElement_get_cols, HTMLFrameSetElement_set_cols},
{"rows", 1, NORMAL, HTMLFrameSetElement_get_rows, HTMLFrameSetElement_set_rows},
{0}};

static JSFunctionSpec HTMLFrameSetElement_methods[] = {
{0}};

static JSPropertySpec HTMLFrameElement_props[] = {
{"frameBorder", 0, NORMAL, HTMLFrameElement_get_frameBorder, HTMLFrameElement_set_frameBorder},
{"longDesc", 1, NORMAL, HTMLFrameElement_get_longDesc, HTMLFrameElement_set_longDesc},
{"marginHeight", 2, NORMAL, HTMLFrameElement_get_marginHeight, HTMLFrameElement_set_marginHeight},
{"marginWidth", 3, NORMAL, HTMLFrameElement_get_marginWidth, HTMLFrameElement_set_marginWidth},
{"name", 4, NORMAL, HTMLFrameElement_get_name, HTMLFrameElement_set_name},
{"noResize", 5, NORMAL, HTMLFrameElement_get_noResize, HTMLFrameElement_set_noResize},
{"scrolling", 6, NORMAL, HTMLFrameElement_get_scrolling, HTMLFrameElement_set_scrolling},
{"src", 7, NORMAL, HTMLFrameElement_get_src, HTMLFrameElement_set_src},
{"contentDocument", 8, NORMAL | JS_PROP_READONLY, HTMLFrameElement_get_contentDocument, NULL},
{0}};

static JSFunctionSpec HTMLFrameElement_methods[] = {
{0}};

static JSPropertySpec HTMLIFrameElement_props[] = {
{"align", 0, NORMAL, HTMLIFrameElement_get_align, HTMLIFrameElement_set_align},
{"frameBorder", 1, NORMAL, HTMLIFrameElement_get_frameBorder, HTMLFrameElement_set_frameBorder},
{"height", 2, NORMAL, HTMLIFrameElement_get_height, HTMLIFrameElement_set_height},
{"longDesc", 3, NORMAL, HTMLIFrameElement_get_longDesc, HTMLIFrameElement_set_longDesc},
{"marginHeight", 4, NORMAL, HTMLIFrameElement_get_marginHeight, HTMLIFrameElement_set_marginHeight},
{"name", 5, NORMAL, HTMLIFrameElement_get_name, HTMLIFrameElement_set_name},
{"scrolling", 6, NORMAL, HTMLIFrameElement_get_scrolling, HTMLIFrameElement_set_scrolling},
{"src", 7, NORMAL, HTMLIFrameElement_get_src, HTMLIFrameElement_set_src},
{"width", 8, NORMAL, HTMLIFrameElement_get_width, HTMLIFrameElement_set_width},
{"contentDocument", 9, NORMAL | JS_PROP_READONLY, HTMLIFrameElement_get_contentDocument, NULL},
{0}};

static JSFunctionSpec HTMLIFrameElement_methods[] = {
{0}};

#define INIT_CLASS(name,proto) name = JSInitClass(cx, glob, proto, &name ## _class, NULL, 0, \
&name ## _props, &name ## _methods, NULL, NULL); if (!name) return NULL;

JSContext *cx
InitContext(JSRuntime *rt, JSObject *glob)
{
	JSObject *DOMImplementation, *Node, *NodeList, *NamedNodeMap, *CharacterData,
		*Attr, *Element, *Text, *Comment, *CDATASection, *DocumentType, *Notation,
		*Entity, *EntityReference, *ProcessingInstruction, *DocumentFragment,
		*Document;
	JSObject *HTMLCollection, *HTMLOptionsCollection, *HTMLDocument, *HTMLElement,
		*HTMLHtmlElement, *HTMLHeadElement, *HTMLLinkElement, *HTMLTitleElement,
		*HTMLMetaElement, *HTMLBaseElement, *HTMLIsIndexElement, *HTMLStyleElement,
		*HTMLBodyElement, *HTMLFormElement, *HTMLSelectElement, *HTMLOptGroupElement,
		*HTMLOptionElement, *HTMLInputElement, *HTMLTextAreaElement, *HTMLButtonElement,
		*HTMLLabelElement, *HTMLFieldSetElement, *HTMLLegendElement, *HTMLUListElement,
		*HTMLOListElement, *HTMLDListElement, *HTMLDirectoryElement, *HTMLMenuElement,
		*HTMLLIElement, *HTMLDivElement, *HTMLParagraphElement, *HTMLHeadingElement,
		*HTMLQuoteElement, *HTMLPreElement, *HTMLBRElement, *HTMLBaseFontElement,
		*HTMLFontElement, *HTMLHRElement, *HTMLModElement, *HTMLAnchorElement,
		*HTMLImageElement, *HTMLObjectElement, *HTMLParamElement, *HTMLAppletElement,
		*HTMLMapElement, *HTMLAreaElement, *HTMLScriptElement, *HTMLTableElement,
		*HTMLTableCaptionElement, *HTMLTableColElement, *HTMLTableSectionElement,
		*HTMLTableRowElement, *HTMLTableCellElement, *HTMLFrameSetElement,
		*HTMLFrameElement, *HTMLIFrameElement;
		
	JSContext *cx = JS_NewContext(rt, 8192);

	if (!cx) return NULL;
	JS_InitStandardClasses(cx, glob);

	INIT_CLASS(DOMImplementation, NULL);
	INIT_CLASS(Node, NULL);
	INIT_CLASS(NodeList, NULL);
	INIT_CLASS(NamedNodeMap, NULL);
	INIT_CLASS(CharacterData, Node);
	INIT_CLASS(Attr, Node);
	INIT_CLASS(Element, Node);
	INIT_CLASS(Text, CharacterData);
	INIT_CLASS(Comment, CharacterData);
	INIT_CLASS(CDATASection, Text);
	INIT_CLASS(DocumentType, Node);
	INIT_CLASS(Notation, Node);
	INIT_CLASS(Entity, Node);
	INIT_CLASS(EntityReference, Node);
	INIT_CLASS(ProcessingInstruction, Node);
	INIT_CLASS(DocumentFragment, Node);
	INIT_CLASS(Document, Node);

	INIT_CLASS(HTMLCollection, NULL);
	INIT_CLASS(HTMLOptionsCollection, NULL);
	INIT_CLASS(HTMLDocument, Document);
	INIT_CLASS(HTMLElement, Element);
	INIT_CLASS(HTMLHtmlElement, HTMLElement);
	INIT_CLASS(HTMLHeadElement, HTMLElement);
	INIT_CLASS(HTMLLinkElement, HTMLElement);
	INIT_CLASS(HTMLTitleElement, HTMLElement);
	INIT_CLASS(HTMLMetaElement, HTMLElement);
	INIT_CLASS(HTMLBaseElement, HTMLElement);
	INIT_CLASS(HTMLIsIndexElement, HTMLElement);
	INIT_CLASS(HTMLStyleElement, HTMLElement);
	INIT_CLASS(HTMLBodyElement, HTMLElement);
	INIT_CLASS(HTMLFormElement, HTMLElement);
	INIT_CLASS(HTMLSelectElement, HTMLElement);
	INIT_CLASS(HTMLOptGroupElement, HTMLElement);
	INIT_CLASS(HTMLOptionElement, HTMLElement);
	INIT_CLASS(HTMLInputElement, HTMLElement);
	INIT_CLASS(HTMLTextAreaElement, HTMLElement);
	INIT_CLASS(HTMLButtonElement, HTMLElement);
	INIT_CLASS(HTMLLabelElement, HTMLElement);
	INIT_CLASS(HTMLFieldSetElement, HTMLElement);
	INIT_CLASS(HTMLLegendElement, HTMLElement);
	INIT_CLASS(HTMLUListElement, HTMLElement);
	INIT_CLASS(HTMLOListElement, HTMLElement);
	INIT_CLASS(HTMLDListElement, HTMLElement);
	INIT_CLASS(HTMLDirectoryElement, HTMLElement);
	INIT_CLASS(HTMLMenuElement, HTMLElement);
	INIT_CLASS(HTMLLIElement, HTMLElement);
	INIT_CLASS(HTMLDivElement, HTMLElement);
	INIT_CLASS(HTMLParagraphElement, HTMLElement);
	INIT_CLASS(HTMLHeadingElement, HTMLElement);
	INIT_CLASS(HTMLQuoteElement, HTMLElement);
	INIT_CLASS(HTMLPreElement, HTMLElement);
	INIT_CLASS(HTMLBRElement, HTMLElement);
	INIT_CLASS(HTMLBaseFontElement, HTMLElement);
	INIT_CLASS(HTMLFontElement, HTMLElement);
	INIT_CLASS(HTMLHRElement, HTMLElement);
	INIT_CLASS(HTMLModElement, HTMLElement);
	INIT_CLASS(HTMLAnchorElement, HTMLElement);
	INIT_CLASS(HTMLImageElement, HTMLElement);
	INIT_CLASS(HTMLObjectElement, HTMLElement);
	INIT_CLASS(HTMLParamElement, HTMLElement);
	INIT_CLASS(HTMLAppletElement, HTMLElement);
	INIT_CLASS(HTMLMapElement, HTMLElement);
	INIT_CLASS(HTMLAreaElement, HTMLElement);
	INIT_CLASS(HTMLScriptElement, HTMLElement);
	INIT_CLASS(HTMLTableElement, HTMLElement);
	INIT_CLASS(HTMLTableCaptionElement, HTMLElement);
	INIT_CLASS(HTMLTableColElement, HTMLElement);
	INIT_CLASS(HTMLTableSectionElement, HTMLElement);
	INIT_CLASS(HTMLTableRowElement, HTMLElement);
	INIT_CLASS(HTMLTableCellElement, HTMLElement);
	INIT_CLASS(HTMLFrameSetElement, HTMLElement);
	INIT_CLASS(HTMLFrameElement, HTMLElement);
	INIT_CLASS(HTMLIFrameElement, HTMLElement);

	return cx;
}
