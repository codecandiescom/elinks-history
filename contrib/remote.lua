-- ELinks-side part of links-remote
-- $Id: remote.lua,v 1.1 2002/05/05 15:10:20 pasky Exp $

-- See script links-remote for explanation what's this about.

----------------------------------------------------------------------
--  User options
----------------------------------------------------------------------

-- File to look in for external url to jump to
   external_url_file = home_dir.."/.links/external.url"


----------------------------------------------------------------------
--  Implementation
----------------------------------------------------------------------

function external_url ()
   fh = openfile (external_url_file, "r")
   aline = current_url ()
   if fh then
      aline = read (fh, "*l")
      closefile (fh)
   else
      print ("Couldn't open outfile")
   end
   return aline
end

   bind_key ("main", "x",
             function () return "goto_url", external_url () end)

-- vim: shiftwidth=4 softtabstop=4
