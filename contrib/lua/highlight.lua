-- Source-code highlighting hook
-- $Id: highlight.lua,v 1.1 2005/03/25 18:40:15 miciah Exp $

function highlight (url, html)
  local ret=nil

  if not highlight_enable then return nil,nil end

-- highlight patches
  if string.find (url, "%.patch$") then
	local tmp = tmpname ()
	writeto (tmp) write (html) writeto()
	html = pipe_read ("(code2html -l patch "..tmp.." - ) 2>/dev/null")
	os.remove(tmp)
	ret = 1
  end

-- Python
  if string.find (url, "%.py$") then
	local tmp = tmpname ()
	writeto (tmp) write (html) writeto()
	html = pipe_read ("(code2html -l python "..tmp.." - ) 2>/dev/null")
	os.remove(tmp)
	ret = 1
  end

-- Perl
  if string.find (url, "%.pl$") then
	local tmp = tmpname ()
	writeto (tmp) write (html) writeto()
	html = pipe_read ("(code2html -l perl "..tmp.." - ) 2>/dev/null")
	os.remove(tmp)
	ret = 1
  end

-- awk
  if string.find (url, "%.awk$") then
	local tmp = tmpname ()
	writeto (tmp) write (html) writeto()
	html = pipe_read ("(code2html -l awk "..tmp.." - ) 2>/dev/null")
	os.remove(tmp)
	ret = 1
  end

-- hightlight C code
    if string.find (url, "%.[ch]$") then
        local tmp = tmpname ()
        writeto (tmp) write (html) writeto ()
        html = pipe_read ("(code2html -l c "..tmp.." - ) 2>/dev/null")
        os.remove (tmp)
        ret = 1
    end

-- shell
    if string.find (url, "%.sh$") then
        local tmp = tmpname ()
        writeto (tmp) write (html) writeto ()
        html = pipe_read ("(code2html -l sh "..tmp.." - ) 2>/dev/null")
        os.remove (tmp)
        ret = 1
    end

    return (ret and html),nil
end

table.insert(pre_format_html_hooks, highlight)
