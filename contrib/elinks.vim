" Vim syntax file
" Language:	ELinks configuration file (elinks.conf)
" Maintainer:	Jonas Fonseca <fonseca@diku.dk>
" Last Update:	07th sep 2002
" 
" Description:	This file covers elinks version 0.4pre15
" 
" Todo:		Add more color words.
"		Support for include command.
"		Improve error highlighting.
"		Highlighting of various stuff inside of strings (\", %).

" $Id: elinks.vim,v 1.1 2002/09/12 17:55:59 pasky Exp $

" For version 5.x: Clear all syntax items
" For version 6.x: Quit when a syntax file was already loaded
if version < 600
  syntax clear
elseif exists("b:current_syntax")
  finish
endif

" This will highlight everything not matched below like an error
syn match	elinksError	/^\s*\zs\S*/

syn match	elinksComment	/\s*#.*$/ contains=elinksTodo,elinksSyntax
syn keyword	elinksTodo	contained TODO NOTE NOT FIXME XXX
syn match	elinksSyntax	contained /[a-zA-Z-0-9\._-]\+\s\(<.*>\|\[.*|.*\]\)/

syn match	elinksAssign	contained /=/

" Set commands:
syn match	elinksSet	/^\s*set/ skipwhite nextgroup=elinksTreeOpt,elinksError

" The '=' following a tree option is included in elinksTreeOpt so it
" controls wether the tree options should be highlighted.
syn match	elinksTreeOpt	/[a-zA-Z-0-9\*\._-]\+\s*=/ skipwhite contains=elinksAssign nextgroup=elinksValue,elinksNumber

syn region	elinksValue	contained start=+"+ms=e end=+"+ contains=elinksEmail,elinksURL,elinksColor,elinksColorErr

syn match	elinksNumber	/\s*-\?\d\+[Mk]\?/

syn match	elinksColorErr	contained /#\x*/
syn match	elinksColor	contained /#\x\{6\}/
syn keyword	elinksColor	contained black red white blue yellow green darkred gray

" Now catch some email addresses and headers (purified version from mail.vim)
syn match	elinksEmail	contained "[a-zA-Z0-9._-]\+@[a-zA-Z0-9.-_]\+"
syn match	elinksURL	contained "\(http\|ftp\)://\w\+[a-zA-Z0-9._-]*"

" Bind commands:
syn match	elinksBind	/^\s*bind/ skipwhite nextgroup=elinksKeymap,elinksKeymapErr

syn match	elinksKeymapErr	contained /"[^"]*"/ skipwhite nextgroup=elinksKey
syn match	elinksKeymap	contained /"\(main\|edit\|menu\)"/ skipwhite nextgroup=elinksKey

syn match	elinksKey	contained /"[^"]\+"/ skipwhite nextgroup=elinksActStr

syn match	elinksActStr	/=\s*"[^"]*"/ contains=elinksAssign,elinksAction

syn keyword	elinksAction	contained add-bookmark back bookmark-manager download end enter file-menu
syn keyword	elinksAction	contained find-next find-next-back goto-url goto-url-current goto-url-current-link
syn keyword	elinksAction	contained header-info home link-menu lua-console menu next-frame open-new-window
syn keyword	elinksAction	contained open-link-in-new-window page-down page-up paste-clipboard previous-frame
syn keyword	elinksAction	contained quit really-quit reload scroll-down scroll-left scroll-right scroll-up
syn keyword	elinksAction	contained search search-back toggle-display-images toggle-display-tables 
syn keyword	elinksAction	contained toggle-html-plain unback view-image zoom-frame jump-to-link document-info
syn keyword     elinksAction    contained cookies-load history-manager enter-reload 
syn keyword	elinksAction	contained up down left right home end backspace delete kill-to-bol kill-to-eol auto-complete
syn keyword	elinksAction	contained enter copy-clipboard cut-clipboard paste-clipboard edit auto-complete-unambiguous
syn keyword	elinksAction	contained left right up down home end page-up page-down

" Define the default highlighting.
" For version 5.7 and earlier: only when not done already
" For version 5.8 and later: only when an item doesn't have highlighting yet
if version >= 508 || !exists("did_elinks_syntax_inits")
  if version < 508
    let did_elinks_syntax_inits = 1
    command -nargs=+ HiLink hi link <args>
  else
    command -nargs=+ HiLink hi def link <args>
  endif

  HiLink elinksComment	 Comment
  HiLink elinksTodo	 Todo
  HiLink elinksSyntax	 SpecialComment
  HiLink elinksColorErr	 elinksError
  HiLink elinksKeymapErr elinksError
  HiLink elinksError	 Error

  " Set links
  HiLink elinksSet	 Keyword
  HiLink elinksTreeOpt	 Identifier
  HiLink elinksAssign	 Operator
  HiLink elinksValue	 String
  HiLink elinksColor	 Type
  HiLink elinksEmail	 Type
  HiLink elinksURL	 Type
  HiLink elinksNumber	 Number

  " Bind links
  HiLink elinksBind	 Keyword
  HiLink elinksKeymap	 Type
  HiLink elinksKey	 Macro
  HiLink elinksActStr	 String
  HiLink elinksAction	 Identifier

  delcommand HiLink
endif

let b:current_syntax = "elinks"
