-- Source-code highlighting hook
-- $Id: highlight.lua,v 1.4 2005/03/25 19:00:46 miciah Exp $

highlight_file_types = {
    patch = "%.patch$",
    python = "%.py$",
    perl = "%.pl$",
    awk = "%.awk$",
    c = "%.[ch]$",
    sh = "%.sh$",
}

function highlight (url, html)
    local ret=nil

    for language,pattern in pairs(highlight_file_types) do
        if string.find (url, pattern) then
            local tmp = tmpname ()
            writeto (tmp) write (html) writeto()
            html = pipe_read ("(code2html -l "..language.." "..tmp.." - ) 2>/dev/null")
            os.remove(tmp)
            ret = 1
        end
    end

    return (ret and html),nil
end

table.insert(pre_format_html_hooks, highlight)
