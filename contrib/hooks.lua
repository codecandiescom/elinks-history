-- Example hooks.lua file, put in ~/.elinks/ as hooks.lua.
-- $Id: hooks.lua,v 1.6 2002/06/30 19:50:56 pasky Exp $

----------------------------------------------------------------------
--  Local configuration
----------------------------------------------------------------------

-- ** IMPORTANT **
-- For security reasons, systems functions are not enabled by default.
-- To do so, uncomment the following line, but be careful about
-- including unknown code.  Individual functions may be disabled by
-- assigning them to `nil'.

    -- enable_systems_functions ()

    -- openfile = nil    -- may open files in write mode
    -- readfrom = nil    -- reading from pipe can execute commands
    -- writeto = nil
    -- appendto = nil
    -- pipe_read = nil
    -- remove = nil
    -- rename = nil
    -- execute = nil
    -- exit = nil

-- Home directory: If you do not enable system functions, you will
-- need to set the following to your home directory.

    home_dir = (getenv and getenv ("HOME")) or "/home/MYSELF"
    hooks_file = home_dir.."/.elinks/hooks.lua"

-- Pausing: When external programs are run, sometimes we need to pause
-- to see the output.  This is the string we append to the command
-- line to do that.  You may customise it if you wish.

    pause = '; echo -ne "\\n\\e[1;32mPress ENTER to continue...\\e[0m"; read'

-- Make ALT="" into ALT="&nbsp;": Makes web pages with superfluous
-- images look better.  However, even if you disable the "Display links
-- to images" option, single space links to such images will appear.
-- To enable, set the following to 1.  If necessary, you can change
-- this while in Links using the Lua Console, then reload the page.
-- See also the keybinding section at the end of the file.

    mangle_blank_alt = nil

-- If you set this to non-`nil', the bookmark addon will be loaded,
-- and actions will be bound to my key bindings.  Change them at the
-- bottom of the file.
-- Note that you need to copy bm.lua (from contrib/) to ~/.elinks/ directory
-- as well.

    bookmark_addon = nil

-- For any other lua script to be loaded, clone following line:

    dofile (home_dir.."/.elinks/script.lua")


----------------------------------------------------------------------
--  case-insensitive gsub
----------------------------------------------------------------------

-- Please note that this is not completely correct yet.
-- It will not handle pattern classes like %a properly.
-- FIXME: Handle pattern classes.

function gisub (s, pat, repl, n)
    pat = gsub (pat, '(%a)', 
	        function (v) return '['..strupper(v)..strlower(v)..']' end)
    if n then
	return gsub (s, pat, repl, n)
    else
	return gsub (s, pat, repl)
    end
end


----------------------------------------------------------------------
--  goto_url_hook
----------------------------------------------------------------------

function match (prefix, url)
    return strsub (url, 1, strlen (prefix)) == prefix
end

function plusify (str)
    return gsub (str, "%s", "+")
end

dumbbookmarks = {
    g  = "http://www.google.com/",
    gg = "http://www.google.com/",
    go = "http://www.google.com/",
    fm = "http://www.freshmeat.net/",
    sf = "http://www.sourceforge.net/",
    dbug = "http://bugs.debian.org/",
    dpkg = "http://packages.debian.org/",
    pycur = "http://www.python.org/doc/current/",
    pydev = "http://www.python.org/dev/doc/devel/",
    pyhelp = "http://starship.python.net/crew/theller/pyhelp.cgi",
    e2 = "http://www.everything2.org/",
}

smartbookmarks = {
    g  = "http://www.google.com/search?q=%s&btnG=Google+Search",
    gg = "http://www.google.com/search?q=%s&btnG=Google+Search",
    go = "http://www.google.com/search?q=%s&btnG=Google+Search",
    fm = "http://www.freshmeat.net/search/?q=%s",
    sf = "http://sourceforge.net/search/?q=%s",
    sfp = "http://sourceforge.net/projects/%s",
    dbug = "http://bugs.debian.org/%s",
    dpkg = "http://packages.debian.org/%s",
    py = "http://starship.python.net/crew/theller/pyhelp.cgi?keyword=%s&version=current",
    pydev = "http://starship.python.net/crew/theller/pyhelp.cgi?keyword=%s&version=devel",
    e2 = "http://www.everything2.org/?node=%s",
    encz = "http://www.slovnik.cz/bin/ecd?ecd_il=1&ecd_vcb=%s&ecd_trn=translate&ecd_trn_dir=0&ecd_lines=15&ecd_hptxt=0",
    czen = "http://www.slovnik.cz/bin/ecd?ecd_il=1&ecd_vcb=%s&ecd_trn=translate&ecd_trn_dir=1&ecd_lines=15&ecd_hptxt=0",
    dict = "http://www.dictionary.com/cgi-bin/dict.pl?db=%%2A&term=%s",
    whatis = "http://uptime.netcraft.com/up/graph/?host=%s",
    -- rfc by number
    rfc = "http://www.rfc-editor.org/rfc/rfc%s.txt",
    -- rfc search
    rfcs = "http://www.rfc-editor.org/cgi-bin/rfcsearch.pl?searchwords=%s&format=http&abstract=abson&keywords=keyon&num=25",
    cr   = "http://www.rfc-editor.org/cgi-bin/rfcsearch.pl?searchwords=%s&format=http&abstract=abson&keywords=keyon&num=25",
    -- Internet Draft search
    rfcid = "http://www.rfc-editor.org/cgi-bin/idsearch.pl?searchwords=%s&format=http&abstract=abson&keywords=keyon&num=25",
    id    = "http://www.rfc-editor.org/cgi-bin/idsearch.pl?searchwords=%s&format=http&abstract=abson&keywords=keyon&num=25",
    draft = "http://www.rfc-editor.org/cgi-bin/idsearch.pl?searchwords=%s&format=http&abstract=abson&keywords=keyon&num=25",
}

function goto_url_hook (url, current_url)
    if dumbbookmarks[url] then
        return dumbbookmarks[url]
    end

    if strfind(url,'%s') or strfind(url, ':') then
        local _,_,nick,val = strfind(url, "^([^%s]+)[:%s]%s*(.-)%s*$")
        if smartbookmarks[nick] then
            val = plusify(val)
            return format(smartbookmarks[nick], val)
        end
    end

    -- Expand ~ to home directories.
    if match ("~", url) then
        if strsub(url, 2, 2) == "/" then    -- ~/foo
            return home_dir..strsub(url, 2)
        else                                -- ~foo/bar
            return "/home/"..strsub(url, 2)
        end
    end

    -- Don't take localhost as directory name
    if match("localhost", url) then
	return "http://"..url
    end

    -- Unmatched.
    return url
end


-----------------------------------------------------------------------
--  follow_url_hook
---------------------------------------------------------------------

function follow_url_hook (url)
    -- Using bookmark addon.
    if bookmark_addon then
	if bm_is_category (url) then
	    return nil
	else
	    return bm_get_bookmark_url (url) or url
	end

    -- Not using bookmark addon.
    else
	return url
    end
end


----------------------------------------------------------------------
--  pre_format_html_hook
----------------------------------------------------------------------

-- Plain strfind (no metacharacters).
function sstrfind (s, pattern)
    return strfind (s, pattern, 1, 1)
end

function pre_format_html_hook (url, html)
    local ret = nil

    -- Handle gzip'd files within reasonable size.
    if strfind (url, "%.gz$") and strlen (html) < 65536 then
        local tmp = tmpname ()
        writeto (tmp) write (html) writeto ()
        html = pipe_read ("(gzip -dc "..tmp.." || cat "..tmp..") 2>/dev/null")
        remove (tmp)
        ret = 1
    end

    -- Mangle ALT="" in IMG tags.
    if mangle_blank_alt then
	local n
	html, n = gisub (html, '(<img.-) alt=""', '%1 alt="&nbsp;"')
	ret = ret or (n > 0)
    end

    -- Fix unclosed INPUT tags.
    if 1 then
        local n
	html, n = gisub (html, '(<input[^>]-[^=]")<', '%1><')
	ret = ret or (n > 0)
    end

    -- Fix unclosed A tags.
    if 1 then
        local n
	html, n = gisub (html, '(<a[^>]-[^=]")<', '%1><')
	ret = ret or (n > 0)
    end

    -- These quick 'n dirty patterns don't maintain proper HTML.

    -- linuxtoday.com
    if sstrfind (url, "linuxtoday.com") then
        if sstrfind (url, "news_story") then
            html = gsub (html, '<TABLE CELLSPACING="0".-</TABLE>', '', 1)
            html = gsub (html, '<TR BGCOLOR="#FFF.-</TR></TABLE>', '', 1)
        else
            html = gsub (html, 'WIDTH="120">\n<TR.+</TABLE></TD>', '>', 1)
        end
        html = gsub (html, '<A HREF="http://www.internet.com.-</A>', '')
        html = gsub (html, "<IFRAME.-</IFRAME>", "")
        -- emphasis in text is lost
        return gsub (html, 'text="#002244"', 'text="#001133"', 1)

    -- linuxgames.com
    elseif sstrfind (url, "linuxgames.com") then
        return gsub (html, "<CENTER>.-</center>", "", 1)

    -- dictionary.com
    elseif sstrfind (url, "dictionary.com/cgi-bin/dict.pl") then
	local t = { t = "" }
	local _, n = gsub (html, "resultItemStart %-%-%>(.-)%<%!%-%- resultItemEnd",
			   function (x) %t.t = %t.t.."<tr><td>"..x.."</td></tr>" end)
	if n == 0 then
	    -- we've already mangled this page before
	    return html
	else
	    return "<html><head><title>Dictionary.com lookup</title></head>"..
		    "<body><table border=0 cellpadding=5>"..t.t.."</table>"..
		    "</body></html>"
	end
    end

    return ret and html
end


----------------------------------------------------------------------
--  Miscellaneous functions, accessed with the Lua Console.
----------------------------------------------------------------------

-- Reload this file (hooks.lua) from within Links.
function reload ()
    dofile (hooks_file)
end

-- Helper function.
function catto (output)
    local doc = current_document_formatted (79)
    if doc then writeto (output) write (doc) writeto () end
end

-- Email the current document, using Mutt (http://www.mutt.org).
-- This only works when called from lua_console_hook, below.
function mutt ()
    local tmp = tmpname ()
    writeto (tmp) write (current_document ()) writeto ()
    tinsert (tmp_files, tmp)
    return "run", "mutt -a "..tmp
end

-- Table of expressions which are recognised by our lua_console_hook.
console_hook_functions = {
    reload	= "reload ()",
    mutt	= mutt,
}

function lua_console_hook (expr)
    local x = console_hook_functions[expr] 
    if type (x) == "function" then
	return x ()
    else
	return "eval", x or expr
    end
end


----------------------------------------------------------------------
--  quit_hook
----------------------------------------------------------------------

-- We need to delete the temporary files that we create.
if not tmp_files then
    tmp_files = {}
end

function quit_hook ()
    if bookmark_addon then
	bm_save_bookmarks ()
    end
    
    if tmp_files and remove then
        tmp_files.n = nil
        for i,v in tmp_files do remove (v) end
    end
end


----------------------------------------------------------------------
--  Examples of keybinding
----------------------------------------------------------------------

-- Bind Ctrl-H to a "Home" page.

--    bind_key ("main", "Ctrl-H",
--	      function () return "goto_url", "http://www.google.com/" end)

-- Bind Alt-p to print.

--    bind_key ("main", "Alt-p", lpr)

-- Bind Alt-m to toggle ALT="" mangling.

    bind_key ("main", "Alt-m",
	      function () mangle_blank_alt = not mangle_blank_alt end)


----------------------------------------------------------------------
--  Bookmark addon
----------------------------------------------------------------------

if bookmark_addon then

    dofile (home_dir.."/.elinks/bm.lua")

    -- Add/change any bookmark options here.

    -- Be careful not to load bookmarks if this script is being
    -- reloaded while in Links, or we will lose unsaved changes.
    if not bm_bookmarks or getn (bm_bookmarks) == 0 then
	bm_load_bookmarks ()
    end

    -- My bookmark key bindings.
--    bind_key ('main', 'a', bm_add_bookmark)
--    bind_key ('main', 's', bm_view_bookmarks)
--    bind_key ('main', 'Alt-e', bm_edit_bookmark)
--    bind_key ('main', 'Alt-d', bm_delete_bookmark)
--    bind_key ('main', 'Alt-k', bm_move_bookmark_up)
--    bind_key ('main', 'Alt-j', bm_move_bookmark_down)

end


-- vim: shiftwidth=4 softtabstop=4
