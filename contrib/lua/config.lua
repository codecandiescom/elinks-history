-- Configuration for hooks.lua file, put in ~/.elinks/ as config.lua.
-- $Id: config.lua,v 1.7 2005/03/27 22:27:08 miciah Exp $

-- ** IMPORTANT **
-- Individual functions may be disabled for security by assigning them
-- to `nil'.

    -- openfile = nil    -- may open files in write mode
    -- readfrom = nil    -- reading from pipe can execute commands
    -- writeto = nil
    -- appendto = nil
    -- pipe_read = nil
    -- remove = nil
    -- rename = nil
    -- execute = nil
    -- exit = nil

-- Home directory:

    home_dir = home_dir or (getenv and getenv ("HOME")) or "/home/MYSELF"
    hooks_file = elinks_home.."/hooks.lua"

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

-- For any other lua script to be loaded (note that you don't need to load
-- hooks.lua here, as it's loaded even when we'll get to here actually; you
-- don't need to load bm.lua here as well, as it gets done in hooks.lua if you
-- will just enable bookmark_addon variable), uncomment and clone following
-- line:

--  dofile (elinks_home.."/script.lua")

-- Highlighting: Uncomment the following line if you want to see highlighted
-- source code.  You need to have code2html installed and set text/html
-- as the MIME-type for .c, .h, .pl, .py, .sh, .awk, .patch extensions
-- in the Options Manager or in elinks.conf

--  dofile (elinks_home.."/highlight.lua")

--  dofile (elinks_home.."/md5checks.lua")
--  dofile (elinks_home.."/remote.lua")
