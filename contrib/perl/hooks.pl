# Example hooks.pl file, put in ~/.elinks/ as hooks.pl.
# $Id: hooks.pl,v 1.32 2005/03/26 14:33:59 pasky Exp $
#
# This file is (c) Apu Nahasapeemapetilon and GPL'd.


use strict;
use warnings;
use diagnostics;


=head1 SAMPLE CONFIGURATION FILE

Save this as ~/.elinks/config.pl :

	bork:       yep     # BORKify Google?
	collapse:   okay    # Collapse all XBEL bookmark folders on exit?
	fortune:    elinks  # *fortune*, *elinks* tip, or *none* on quit?
	googlebeta: hell no # I miss DejaNews...
	gotosearch: not yet # Don't use this yet.  It's broken.
	ipv6:       sure    # IPV4 or 6 address blocks with "ip" prefix?
	language:   english # "bf nl en" still works, but now "bf nl" does too
	news:       msnbc   # Agency to use for "news" and "n" prefixes
	search:     elgoog  # Engine for (search|find|www|web|s|f|go) prefixes
	usenet:     google  # *google* or *standard* view for news:// URLs
	weather:    cnn     # Server for "weather" and "w" prefixes

	# news:    bbc, msnbc, cnn, fox, google, yahoo, reuters, eff, wired,
	#          slashdot, newsforge, usnews, newsci, discover, sciam
	# search:  elgoog, google, yahoo, ask jeeves, a9, altavista, msn, dmoz,
	#          dogpile, mamma, webcrawler, netscape, lycos, hotbot, excite
	# weather: weather underground, google, yahoo, cnn, accuweather,
	#          ask jeeves

=cut

=head1 PREFIXES

Don't call the prefixes "dumb", they hate that!  Rather, "interactivity
challenged". (Such politically correct names always appeared to me to be
so much more insulting... --pasky ;-)

prefixes: bugmenot, bored, random, bofh, xyzzy, jack or handey
smart prefixes:
     web search:
          default engine:  search, find, www, web, s, f, go
                           also, anything in quotes with no prefix
          other:
               google:     g or google
               yahoo:      y or yahoo
               ask jeeves: ask or jeeves
               amazon a9:  a9
               altavista:  av or altavista
               microsoft:  msn or microsoft
               dmoz:       dmoz, odp, mozilla
               dogpile:    dp or dogpile
               mamma:      ma or mamma
               webcrawler: wc or webcrawler
               netscape:   ns or netscape
               lycos:      ly or lycos
               hotbot:     hb or hotbot
               excite:     ex or excite
               elgoog:     eg, elgoog, hcraes, dnif, bew, og
     news:
          default agency:  n, news
          other:
               British Broadcasting Corporation: bbc
               MSNBC: msnbc
               Cable News Network: cnn
               FOXNews: fox
               Google News: gn
               Yahoo News: yn
               Reuters: rs or reuters
               Electronic Frontier Foundation: eff
               Wired: wd or wired
               Slashdot: /. or sd or slashdot
               NewsForge: nf or newsforge
               U.S.News & World Report: us or usnews
               New Scientist: newsci or nsci
               Discover Magazine: dm
               Scientific American: sa or sciam
     locators:
          Internet Movie Database: imdb, movie, or flick
          US zip code search: zip or usps (# or address)
          IP address locator / address space: ip
          WHOIS / TLD list: whois (current url or specified)
          Request for Comments: rfc (# or search)
          weather: w or weather
          Yahoo! Finance / NASD Regulation: stock, ticker, or quote
          Snopes: ul, urban, or legend
          torrent search / ISOHunt: bt, torrent, or bittorrent
          Wayback Machine: ia, ar, arc, or archive (current url or specified)
          software:
               Freshmeat: fm or freshmeat
               SourceForge: sf or sourceforge
               Savannah: sv or savannah
               Gna!: gna
          Netcraft Uptime Survey: whatis or uptime (current url or specified)
          Who's Alive and Who's Dead: alive or dead
          Google Library / Project Gutenberg: book or read
          Internet Public Library: ipl
     other:
          usenet: deja, gg, groups, gr, nntp, usenet, nn
          page translation: babelfish, babel, bf, translate, trans, or b
          MirrorDot: md or mirrordot
          Coral cache: cc, coral, or nyud (requires URL)
          page validators: vhtml or vcss (current url or specified)
elinks: el / elinks, bz / bug (# or search optional), doc(|s|umentation), faq

=cut


################################################################################
### goto_url_hook ##############################################################
sub goto_url_hook
{
	my $url = shift;
	my $current_url = shift;

	# "bugmenot" (no blood today, thank you)
	if ($url =~ '^bugmenot$' && $current_url) {
		(undef, $current_url) = $current_url =~ /^(.*):\/\/(.*)/;
		$url = 'http://bugmenot.com/view.php?url=' . $current_url;
		return $url;
	}

	# Random URL generator
	if ($url =~ '^bored$' || $url =~ '^random$') {
		my $word; # You can say *that* again...
		srand();

		open FILE, '</usr/share/dict/words'
			or open FILE, '</usr/share/dict/linux.words'
			or open FILE, '</usr/dict/words'
			or open FILE, '</usr/dict/linux.words'
			or open FILE, '</usr/share/dict/yawl.list'
			or open FILE, $ENV{"HOME"} . '/.elinks/elinks.words'
			or return 'http://google.com/webhp?hl=xx-bork';
		rand($.) < 1 && ($word = $_) while <FILE>;
		close FILE;

		($word) = $word =~ /(.*)/;
		$url = 'http://' . lc($word) . '.com';
		return $url;
	}


	# Search engines

	my ($search) = $url =~ /^[a-z0-9]+\s+(.*)/;

	if ($url =~ /^(search|find|www|web|s|f|go)(| .*)$/) {
		return search(loadrc('search'), $search);
	}
	if ($url =~ s/("|\'|')(.+)$/$2/) {
		return search(loadrc('search'), $url);
	}

	if ($url =~ '^(eg|elgoog|hcraes|dnif|bew|og)(| .*)$'
	    or $url =~ '^(g|google)(| .*)$'
	    or $url =~ '^(y|yahoo)(| .*)$'
	    or $url =~ '^(ask|jeeves)(| .*)$'
	    or $url =~ '^a9(| .*)$'
	    or $url =~ '^(av|altavista)(| .*)$'
	    or $url =~ '^(msn|microsoft)(| .*)$'
	    or $url =~ '^(dmoz|odp|mozilla)(| .*)$'
	    or $url =~ '^(dp|dogpile|dp)(| .*)$'
	    or $url =~ '^(ma|mamma)(| .*)$'
	    or $url =~ '^(wc|webcrawler)(| .*)$'
	    or $url =~ '^(ns|netscape)(| .*)$'
	    or $url =~ '^(ly|lycos)(| .*)$'
	    or $url =~ '^(hb|hotbot)(| .*)$'
	    or $url =~ '^(ex|excite)(| .*)$') {
		my $engine = $url;
		$url = search("elgoog",         $search) if ($engine =~ '^(eg|elgoog|hcraes|dnif|bew|og)(| .*)$');
		$url = search("google",         $search) if ($engine =~ '^(g|google)(| .*)$');
		$url = search("yahoo",          $search) if ($engine =~ '^(y|yahoo)(| .*)$');
		$url = search("ask jeeves",     $search) if ($engine =~ '^(ask|jeeves)(| .*)$');
		$url = search("a9",             $search) if ($engine =~ '^a9(| .*)$');
		$url = search("altavista",      $search) if ($engine =~ '^(av|altavista)(| .*)$');
		$url = search("msn",            $search) if ($engine =~ '^(msn|microsoft)(| .*)$');
		$url = search("dmoz",           $search) if ($engine =~ '^(dmoz|odp|mozilla)(| .*)$');
		$url = search("dogpile",        $search) if ($engine =~ '^(dp|dogpile)(| .*)$');
		$url = search("mamma",          $search) if ($engine =~ '^(ma|mamma)(| .*)$');
		$url = search("webcrawler",     $search) if ($engine =~ '^(wc|webcrawler)(| .*)$');
		$url = search("netscape",       $search) if ($engine =~ '^(ns|netscape)(| .*)$');
		$url = search("lycos",          $search) if ($engine =~ '^(ly|lycos)(| .*)$');
		$url = search("hotbot",         $search) if ($engine =~ '^(hb|hotbot)(| .*)$');
		$url = search("excite",         $search) if ($engine =~ '^(ex|excite)(| .*)$');
		return $url;
	}


	# Google Groups (DejaNews)
	if ($url =~ '^(deja|gg|groups|gr|nntp|usenet|nn)(| .*)$') {
		my ($search) = $url =~ /^[a-z]* (.*)/;
		my $beta = "groups.google.co.uk";
		$beta = "groups-beta.google.com" unless (loadrc("googlebeta") ne "yes");

		my $bork = "";
		if ($search) {
			$bork = "&hl=xx-bork" unless (loadrc("bork") ne "yes");
			$url = 'http://' . $beta . '/groups?q=' . $search . $bork;
		} else {
			$bork = "/groups?hl=xx-bork" unless (loadrc("bork") ne "yes");
			$url = 'http://' . $beta . $bork;
		}
		return $url;
	}

	# MirrorDot
	if ($url =~ '^(mirrordot|md)(| .*)$') {
		my ($slashdotted) = $url =~ /^[a-z]* (.*)/;
		if ($slashdotted) {
			$url = 'http://mirrordot.com/find-mirror.html?' . $slashdotted;
		} else {
			$url = 'http://mirrordot.com';
		}
		return $url;
	}

	# The Bastard Operator from Hell
	if ($url =~ '^bofh$') {
		$url = 'http://prime-mover.cc.waikato.ac.nz/Bastard.html';
		return $url;
	}

	# Coral cache <URL>
	if ($url =~ '^(coral|cc|nyud)( .*)$') {
		my ($cache) = $url =~ /^[a-z]* (.*)/;
		$cache =~ s/^http:\/\///;
		($url) = $cache =~ s/\//.nyud.net:8090\//;
		$url = 'http://' . $cache;
		return $url;
	}

	# Babelfish ("babelfish german english"  or  "bf de en")
	if (($url =~ '^(babelfish|babel|bf|translate|trans|b)(| [a-zA-Z]* [a-zA-Z]*)$')
	    or ($url =~ '^(babelfish|babel|bf|translate|trans|b)(| [a-zA-Z]*(| [a-zA-Z]*))$'
	        and (loadrc("language") ne "no") and $current_url))
	{
		$url = 'http://babelfish.altavista.com' if ($url =~ /^[a-z]*$/);
		if ($url =~ /^[a-z]* /) {
			my $tongue = loadrc("language");
			$url = $url . " " . $tongue if ($tongue ne "no" and $url !~ /^[a-z]* [a-zA-Z]* [a-zA-Z]*$/);
			$url =~ s/ chinese/ zt/i;
			$url =~ s/ dutch/ nl/i;
			$url =~ s/ english/ en/i;
			$url =~ s/ french/ fr/i;
			$url =~ s/ german/ de/i;
			$url =~ s/ greek/ el/i;
			$url =~ s/ italian/ it/i;
			$url =~ s/ japanese/ ja/i;
			$url =~ s/ korean/ ko/i;
			$url =~ s/ portugese/ pt/i;
			$url =~ s/ russian/ ru/i;
			$url =~ s/ spanish/ es/i;

			my ($from_language, $to_language) = $url =~ /^[a-z]* (.*) (.*)$/;
			($current_url) = $current_url =~ /^.*:\/\/(.*)/;
			$url = 'http://babelfish.altavista.com/babelfish/urltrurl?lp='
			       . $from_language . '_' . $to_language . '&url=' . $current_url;
		}
		return $url;
	}

	# XYZZY
	if ($url =~ '^xyzzy$') {
		# $url = 'http://sundae.triumf.ca/pub2/cave/node001.html';
		srand();
		my $yzzyx;
		my $xyzzy = int(rand(6));
		$yzzyx = 1   if ($xyzzy == 0); # Colossal Cave Adventure
		$yzzyx = 227 if ($xyzzy == 1); # Zork Zero: The Revenge of Megaboz
		$yzzyx = 3   if ($xyzzy == 2); # Zork I: The Great Underground Empire
		$yzzyx = 4   if ($xyzzy == 3); # Zork II: The Wizard of Frobozz
		$yzzyx = 5   if ($xyzzy == 4); # Zork III: The Dungeon Master
		$yzzyx = 6   if ($xyzzy == 5); # Zork: The Undiscovered Underground
		$url = 'http://ifiction.org/games/play.php?game=' . $yzzyx;
		return $url;
	}

	# News
	if ($url =~ '^(news|n)(| .*)$'
	    or $url =~ '^bbc(| .*)$'
	    or $url =~ '^msnbc(| .*)$'
	    or $url =~ '^cnn(| .*)$'
	    or $url =~ '^fox(| .*)$'
	    or $url =~ '^gn(| .*)$'
	    or $url =~ '^yn(| .*)$'
	    or $url =~ '^(reuters|rs)(| .*)$'
	    or $url =~ '^eff(| .*)$'
	    or $url =~ '^(wired|wd)(| .*)$'
	    or $url =~ '^(\/\.|slashdot|sd)(| .*)$'
	    or $url =~ '^(newsforge|nf)(| .*)$'
	    or $url =~ '^(us|usnews)(| .*)$'
	    or $url =~ '^(nsci|newsci)(| .*)$'
	    or $url =~ '^dm(| .*)$'
	    or $url =~ '^(sa|sciam)(| .*)$')
	{
		my ($search) = $url =~ /^[a-z0-9\/\.]* (.*)/;
		my (%news_, $news_);
		$news_{"bbc"}       = 'http://news.bbc.co.uk';
		# The bastard child of Microsoft and the National Broadcasting Corporation
		$news_{"msnbc"}     = 'http://msnbc.com';
		$news_{"cnn"}       = 'http://cnn.com';
		$news_{"fox"}       = 'http://foxnews.com';
		$news_{"google"}    = 'http://news.google.com';
		$news_{"yahoo"}     = 'http://news.yahoo.com';
		$news_{"reuters"}   = 'http://reuters.com';
		$news_{"eff"}       = 'http://eff.org';
		$news_{"wired"}     = 'http://wired.com';
		$news_{"slashdot"}  = 'http://slashdot.org';
		$news_{"newsforge"} = 'http://newsforge.com';
		$news_{"usnews"}    = 'http://usnews.com';
		$news_{"newsci"}    = 'http://newscientist.com';
		$news_{"discover"}  = 'http://discover.com';
		$news_{"sciam"}     = 'http://sciam.com';

		if ($search) {
			$news_{"bbc"}       = 'http://newssearch.bbc.co.uk/cgi-bin/search/results.pl?q=' . $search;
			$news_{"msnbc"}     = 'http://msnbc.msn.com/?id=3053419&action=fulltext&querytext=' . $search;
			$news_{"cnn"}       = 'http://search.cnn.com/pages/search.jsp?query=' . $search;
			$news_{"fox"}       = 'http://search.foxnews.com/info.foxnws/redirs_all.htm?pgtarg=wbsdogpile&qkw='
			                      . $search;
			$news_{"google"}    = 'http://news.google.com/news?q=' . $search;
			$news_{"yahoo"}     = 'http://news.search.yahoo.com/search/news/?p=' . $search;
			$news_{"reuters"}   = 'http://reuters.com/newsSearchResultsHome.jhtml?query=' . $search;
			$news_{"eff"}       = 'http://google.com/search?sitesearch=' . $news_{"eff"} . '&q=' . $search;
			$news_{"wired"}     = 'http://search.wired.com/wnews/default.asp?query=' . $search;
			$news_{"slashdot"}  = 'http://slashdot.org/search.pl?query=' . $search;
			$news_{"newsforge"} = 'http://newsforge.com/search.pl?query=' . $search;
			$news_{"usnews"}    = 'http://www.usnews.com/search/Search?keywords=' . $search;
			$news_{"newsci"}    = 'http://www.newscientist.com/search.ns?doSearch=true&articleQuery.queryString='
			                      . $search;
			$news_{"discover"}  = 'http://www.discover.com/search-results/?searchStr=' . $search;
			$news_{"sciam"}     = 'http://sciam.com/search/index.cfm?QT=Q&SC=Q&Q=' . $search;
		}

		my $agency = $url;
		$url = $news_{"bbc"}; # default
		$url = $news_{loadrc("news")} if $news_{loadrc("news")};
		$url = $news_{"bbc"}       if ($agency =~ '^bbc(| .*)$');
		$url = $news_{"msnbc"}     if ($agency =~ '^msnbc(| .*)$');
		$url = $news_{"cnn"}       if ($agency =~ '^cnn(| .*)$');
		$url = $news_{"fox"}       if ($agency =~ '^fox(| .*)$');
		$url = $news_{"google"}    if ($agency =~ '^gn(| .*)$');
		$url = $news_{"yahoo"}     if ($agency =~ '^yn(| .*)$');
		$url = $news_{"reuters"}   if ($agency =~ '^(reuters|rs)(| .*)$');
		$url = $news_{"eff"}       if ($agency =~ '^eff(| .*)$');
		$url = $news_{"wired"}     if ($agency =~ '^(wired|wd)(| .*)$');
		$url = $news_{"slashdot"}  if ($agency =~ '^(\/\.|slashdot|sd)(| .*)$');
		$url = $news_{"newsforge"} if ($agency =~ '^(newsforge|nf)(| .*)$');
		$url = $news_{"usnews"}    if ($agency =~ '^(us|usnews)(| .*)$');
		$url = $news_{"newsci"}    if ($agency =~ '^(nsci|newsci)(| .*)$');
		$url = $news_{"discover"}  if ($agency =~ '^dm(| .*)$');
		$url = $news_{"sciam"}     if ($agency =~ '^(sa|sciam)(| .*)$');
		return $url;
	}

	# ...and now, Deep Thoughts.  by Jack Handey
	if ($url =~ '^(jack|handey)$') {
		$url = 'http://glug.com/handey';
		return $url;
	}

	# Locators
	if ($url =~ '^(imdb|movie|flick)(| .*)$'
	    or $url =~ '^(zip|usps)(| .*)$'
	    or $url =~ '^ip(| .*)$'
	    or $url =~ '^whois(| .*)$'
	    or $url =~ '^rfc(| .*)$'
	    or $url =~ '^(weather|w)(| .*)$'
	    or $url =~ '^(stock|ticker|quote)(| .*)$'
	    or $url =~ '^(urban|legend|ul)(| .*)$'
	    or $url =~ '^(bittorrent|torrent|bt)(| .*)$'
	    or $url =~ '^(archive|arc|ar|ia)(| .*)$'
	    or $url =~ '^(freshmeat|fm)(| .*)$'
	    or $url =~ '^(sourceforge|sf)(| .*)$'
	    or $url =~ '^(savannah|sv)(| .*)$'
	    or $url =~ '^gna(| .*)$'
	    or $url =~ '^(whatis|uptime)(| .*)$'
	    or $url =~ '^(alive|dead)(| .*)$'
	    or $url =~ '^(book|read)(| .*)$'
	    or $url =~ '^ipl(| .*)$') {
		my ($thingy) = $url =~ /^[a-z]* (.*)/;
		my ($domain) = $current_url =~ /([a-z0-9-]+\.(com|net|org|edu|gov|mil))/;

		my $whois = 'http://reports.internic.net/cgi/whois?type=domain&whois_nic=';
		my $locator_imdb        = 'http://imdb.com';
		my $locator_zip         = 'http://usps.com';
		my $ipv                 = "ipv4-address-space"; $ipv = "ipv6-address-space" if loadrc("ipv6") eq "yes";
			my $locator_ip  = 'http://www.iana.org/assignments/' . $ipv;
		my $locator_whois       = 'http://www.iana.org/cctld/cctld-whois.htm';
			$locator_whois      = $whois . $domain if $domain;
		my $locator_rfc         = 'http://ietf.org';
		my $locator_weather     = 'http://weather.noaa.gov';
		my $locator_stock       = 'http://nasdr.com';
		my $locator_bs          = 'http://snopes.com';
		my $locator_torrent     = 'http://isohunt.com';
		my $locator_archive     = 'http://web.archive.org/web/*/' . $current_url;
		my $locator_freshmeat   = 'http://freshmeat.net';
		my $locator_sourceforge = 'http://sourceforge.net';
		my $locator_savannah    = 'http://savannah.nongnu.org';
		my $locator_gna         = 'http://gna.org';
		my $locator_whatis      = 'http://uptime.netcraft.com';
			$locator_whatis     = 'http://uptime.netcraft.com/up/graph/?host=' . $domain if $domain;
		my $locator_dead        = 'http://www.whosaliveandwhosdead.com';
		my $locator_book        = 'http://gutenberg.org';
		my $locator_ipl         = 'http://ipl.org';

		if ($thingy) {
			$locator_imdb        = 'http://imdb.com/Find?select=All&for=' . $thingy;
			$locator_zip         = 'http://zip4.usps.com/zip4/zip_responseA.jsp?zipcode=' . $thingy;
				$locator_zip     = 'http://zipinfo.com/cgi-local/zipsrch.exe?zip=' . $thingy if $thingy !~ '^[0-9]*$';
			$locator_ip          = 'http://melissadata.com/lookups/iplocation.asp?ipaddress=' . $thingy;
			$locator_whois       = $whois . $thingy;
			$locator_rfc         = 'http://rfc-editor.org/cgi-bin/rfcsearch.pl?num=37&searchwords=' . $thingy;
				$locator_rfc     = 'http://ietf.org/rfc/rfc' . $thingy . '.txt' unless $thingy !~ '^[0-9]*$';
			my $weather          = loadrc("weather");
				$locator_weather = 'http://wunderground.com/cgi-bin/findweather/getForecast?query=' . $thingy;
				$locator_weather = 'http://google.com/search?q=weather+"' . $thingy . '"' if $weather eq 'google';
				$locator_weather = 'http://search.yahoo.com/search?p=weather+"' . $thingy . '"' if $weather eq 'yahoo';
				$locator_weather = 'http://weather.cnn.com/weather/search?wsearch=' . $thingy if $weather eq 'cnn';
				$locator_weather = 'http://wwwa.accuweather.com/adcbin/public/us_getcity.asp?zipcode=' . $thingy if $weather eq 'accuweather';
				$locator_weather = 'http://web.ask.com/web?&q=weather ' . $thingy if $weather =~ '^(ask|jeeves|ask jeeves)$';
			$locator_stock       = 'http://finance.yahoo.com/l?s=' . $thingy;
			$locator_bs          = 'http://search.atomz.com/search/?sp-a=00062d45-sp00000000&sp-q=' . $thingy;
			my $bork = ""; $bork = "&hl=xx-bork" unless (loadrc("bork") ne "yes");
				$locator_torrent = 'http://google.com/search?q=filetype:torrent ' . $thingy . $bork;
			$locator_archive     = 'http://web.archive.org/web/*/' . $thingy;
			$locator_freshmeat   = 'http://freshmeat.net/search/?q=' . $thingy;
			$locator_sourceforge = 'http://sourceforge.net/search/?q=' . $thingy;
			$locator_savannah    = 'http://savannah.nongnu.org/search/?type_of_search=soft&words=' . $thingy;
			$locator_gna         = 'https://gna.org/search/?type_of_search=soft&words=' . $thingy;
			$locator_whatis      = 'http://uptime.netcraft.com/up/graph/?host=' . $thingy;
			$locator_dead        = 'http://google.com/search?btnI&sitesearch=' . $locator_dead . '&q=' . $thingy;
			$locator_book        = 'http://google.com/search?q=book+"' . $thingy . '"';
			$locator_ipl         = 'http://ipl.org/div/searchresults/?words=' . $thingy;
		}
		$url = $locator_imdb        if ($url =~ '^(imdb|movie|flick)(| .*)$');
		$url = $locator_zip         if ($url =~ '^(zip|usps)(| .*)$');
		$url = $locator_ip          if ($url =~ '^ip(| .*)$');
		$url = $locator_whois       if ($url =~ '^whois(| .*)$');
		$url = $locator_rfc         if ($url =~ '^rfc(| .*)$');
		$url = $locator_weather     if ($url =~ '^(weather|w)(| .*)$');
		$url = $locator_stock       if ($url =~ '^(stock|ticker|quote)(| .*)$');
		$url = $locator_bs          if ($url =~ '^(urban|legend|ul)(| .*)$');
		$url = $locator_torrent     if ($url =~ '^(bittorrent|torrent|bt)(| .*)$');
		$url = $locator_archive     if ($url =~ '^(archive|arc|ar|ia)(| .*)$');
		$url = $locator_freshmeat   if ($url =~ '^(freshmeat|fm)(| .*)$');
		$url = $locator_sourceforge if ($url =~ '^(sourceforge|sf)(| .*)$');
		$url = $locator_savannah    if ($url =~ '^(savannah|sv)(| .*)$');
		$url = $locator_gna         if ($url =~ '^gna(| .*)$');
		$url = $locator_whatis      if ($url =~ '^(whatis|uptime)(| .*)$');
		$url = $locator_dead        if ($url =~ '^(alive|dead)(| .*)$');
		$url = $locator_book        if ($url =~ '^(book|read)(| .*)$');
		$url = $locator_ipl         if ($url =~ '^ipl(| .*)$');
		return $url;
	}

	# Page validators [<URL>]
	if ($url =~ '^vhtml(| .*)$' or $url =~ '^vcss(| .*)$') {
		my ($page) = $url =~ /^.* (.*)/;
		$page = $current_url unless $page;
		$url = 'http://validator.w3.org/check?uri=' . $page if $url =~ 'html';
		$url = 'http://jigsaw.w3.org/css-validator/validator?uri=' . $page if $url =~ 'css';
		return $url;
	}

	# There's no place like home
	if ($url =~ '^(el(|inks)|b(ug(|s)|z)(| .*)|doc(|umentation|s)|faq)$') {
		my ($bug) = $url =~ /^.* (.*)/;
		if ($url =~ '^b') {
			my $bugzilla = 'http://bugzilla.elinks.or.cz';
			if (not $bug) {
				$url = $bugzilla;
			} elsif ($bug =~ '^[0-9]*$') {
				$url = $bugzilla . '/show_bug.cgi?id=' . $bug;
			} else {
				$url = $bugzilla . '/buglist.cgi?short_desc_type=allwordssubstr&short_desc=' . $bug;
			}
		} else {
			my $doc = '';
			$doc = '/documentation' if $url =~ '^doc';
			$doc = '/faq.html' if $url =~ '^faq$';
			$url = 'http://elinks.or.cz' . $doc;
		}
		return $url;
	}

	# Anything not otherwise useful could be a search
	if ($current_url and loadrc("gotosearch") eq "yes") {
		$url = search(loadrc("search"), $url);
	}
	return $url;
}


################################################################################
### follow_url_hook ############################################################
sub follow_url_hook
{
	my $url = shift;

	# Bork! Bork! Bork!
	if ($url =~ 'google\.com') {
		if (loadrc("bork") eq "yes") {
			if ($url =~ '^http://(|www\.|search\.)google\.com(|/search)(|/)$') {
				$url = 'http://google.com/webhp?hl=xx-bork';
			} elsif ($url =~ '^http://(|www\.)groups\.google\.com(|/groups)(|/)$'
			         or $url =~ '^http://(|www\.|search\.)google\.com/groups(|/)$') {
				$url = 'http://google.com/groups?hl=xx-bork';
			}
		}
		return $url;
	}

	# NNTP?  Try Google Groups
	if ($url =~ '^(nntp|news):' and loadrc("usenet") ne "standard") {
		my $beta = "groups.google.co.uk";
		$beta = "groups-beta.google.com" unless (loadrc("googlebeta") ne "yes");
		$url =~ s/\///g;
		my ($group) = $url =~ /[a-zA-Z]:(.*)/;
		my $bork = "";
		$bork = "hl=xx-bork&" unless (loadrc("bork") ne "yes");
		$url = 'http://' . $beta . '/groups?' . $bork . 'group=' . $group;
		return $url;
	}

	return $url;
}


################################################################################
### pre_format_html_hook #######################################################
sub pre_format_html_hook
{
	my $url = shift;
	my $html = shift;

	# /. sanitation
	if ($url =~ 'slashdot\.org') {
#		$html =~ s/^<!-- Advertisement code. -->.*<!-- end ad code -->$//sm;
#		$html =~ s/<iframe.*><\/iframe>//g;
#		$html =~ s/<B>Advertisement<\/B>//;
	}

	# Yes, I heard you the first time
	if ($url =~ 'google\.com') {
		$html =~ s/Teep: In must broosers yuoo cun joost heet zee retoorn key insteed ooff cleecking oon zee seerch boottun\. Bork bork bork!//;
		$html =~ s/Tip:<\/font> Save time by hitting the return key instead of clicking on "search"/<\/font>/;
	}

	# SourceForge ad smasher
	if ($url =~ 'sourceforge\.net') {
		$html =~ s/<!-- AD POSITION \d+ -->.*?<!-- END AD POSITION \d+ -->//smg;
		$html =~ s/<b>&nbsp\;&nbsp\;&nbsp\;Site Sponsors<\/b>//g;
	}

	# GMail has obviously never met ELinks
	if ($url =~ 'gmail\.google\.com') {
		$html =~ s/^<b>For a better Gmail experience, use a.+?Learn more<\/a><\/b>$//sm;
	}

	# Demoronizer
	$html =~ s/Ñ/\&mdash;/g;
	$html =~ s/\&#252/ü/g;
	$html =~ s/\&#039/'/g;

	return $html;
}


################################################################################
### proxy_for_hook #############################################################
sub proxy_for_hook
{
	my $url = shift;

	# no proxy for local files
	if ($url =~ '^(file://|(http://|)(localhost|127\.0\.0\.1)(/|:|$))') {
		return "";
	}

	return;
}


################################################################################
### quit_hook ##################################################################
sub quit_hook
{
	# Collapse XBEL bookmark folders (obsoleted by bookmarks.folder_state)

	my $bookmarkfile = $ENV{'HOME'} . '/.elinks/bookmarks.xbel';
	if (-f $bookmarkfile and loadrc('collapse') eq 'yes') {
		open BOOKMARKS, "+<$bookmarkfile";
		my $bookmark;
		while (<BOOKMARKS>) {
			s/<folder folded="no">/<folder folded="yes">/;
			$bookmark .= $_;
		}
		seek(BOOKMARKS, 0, 0);
		print BOOKMARKS $bookmark;
		truncate(BOOKMARKS, tell(BOOKMARKS));
		close BOOKMARKS;
	}


	# Words of wisdom from ELinks the Sage

	if (loadrc('fortune') eq 'fortune') {
		system('echo ""; fortune -sa 2>/dev/null');
		die;
	}
	die if (loadrc('fortune') =~ /^(none|quiet)$/);

	my $cookiejar = 'elinks.fortune';
	my $ohwhynot = `ls /usr/share/doc/elinks*/$cookiejar 2>/dev/null`;
	open COOKIES, $ENV{"HOME"} . '/.elinks/' . $cookiejar
		or open COOKIES, '/etc/elinks/' . $cookiejar
		or open COOKIES, '/usr/share/elinks/' . $cookiejar
		or open COOKIES, $ohwhynot
		or die system('echo ""; fortune -sa 2>/dev/null');

	my (@line, $fortune);
	$line[0] = 0;
	while (<COOKIES>) {
		$line[$#line + 1] = tell if /^%$/;
	}
	srand();
	while (not $fortune) {
		# We don't want the last element of the $line array since that
		# is the trailing % in the fortunes file.
		seek(COOKIES, $line[int rand($#line)], 0);
		while (<COOKIES>) {
			last if /^%$/;
			$fortune .= $_;
		}
	}
	close COOKIES;

	print "\n", $fortune;
}


################################################################################
### Configuration ##############################################################
sub loadrc
{
	my ($preference) = @_;
	my $configperl = $ENV{'HOME'} . '/.elinks/config.pl';
	my $answer = 'no';

	open RC, "<$configperl" or return $answer;
	while (<RC>) {
		s/\s*#.*$//;
		next unless (m/(\S+)\s*:\s*(\S+)/);
		my $setting = $1;
		my $switch = $2;
		next unless ($setting eq $preference);

		if ($switch =~ /^(yes|1|on|yea|yep|sure|ok|okay|yeah|why.*not)$/) {
			$answer = "yes";
		} elsif ($switch =~ /^(no|0|off|nay|nope|nah|hell.*no)$/) {
			$answer = "no";
		} else {
			$answer = lc($switch);
		}
	}
	close RC;

	return $answer;
}


################################################################################
### Search engines #############################################################

my %search_engines_ = (
	"elgoog" => {
		home => 'http://alltooflat.com/geeky/elgoog/m/index.cgi',
		search => 'http://alltooflat.com/geeky/elgoog/m/index.cgi?page=%2fsearch&cgi=get&q='
	},
	"google" => {
		home => 'http://google.com!bork!',
		search => 'http://google.com/search?!bork!q='
	},
	"yahoo" => {
		home => 'http://yahoo.com',
		search => 'http://search.yahoo.com/search?p='
	},
	"ask jeeves" => {
		home => 'http://ask.com',
		search => 'http://web.ask.com/web?q='
	},
	"a9" => {
		home => 'http://a9.com',
		search => 'http://a9.com/?q='
	},
	"altavista" => {
		home => 'http://altavista.com',
		search => 'http://altavista.com/web/results?q='
	},
	"msn" => {
		home => 'http://msn.com',
		search => 'http://search.msn.com/results.aspx?q='
	},
	"dmoz" => {
		home => 'http://dmoz.org',
		search => 'http://search.dmoz.org/cgi-bin/search?search='
	},
	"dogpile" => {
		home => 'http://dogpile.com',
		search => 'http://dogpile.com/info.dogpl/search/web/'
	},
	"mamma" => {
		home => 'http://mamma.com',
		search => 'http://mamma.com/Mamma?query='
	},
	"webcrawler" => {
		home => 'http://webcrawler.com',
		search => 'http://webcrawler.com/info.wbcrwl/search/web/'
	},
	"netscape" => {
		home => 'http://search.netscape.com',
		search => 'http://channels.netscape.com/ns/search/default.jsp?query='
	},
	"lycos" => {
		home => 'http://lycos.com',
		search => 'http://search.lycos.com/default.asp?query='
	},
	"hotbot" => {
		home => 'http://hotbot.com',
		search => 'http://hotbot.com/default.asp?query='
	},
	"excite" => {
		home => 'http://search.excite.com',
		search => 'http://search.excite.com/info.xcite/search/web/'
	},
);

sub search
{
	my ($engine, $search) = @_;

	# Google is the default, Google is the best!
	$engine = 'google' unless $search_engines_{$engine};
	my $url = $search_engines_{$engine};

	if ($engine eq 'google') {
		my $bork = '';
		if (loadrc('bork') eq 'yes') {
			if (not $search) {
				$bork = "/webhp?hl=xx-bork";
			} else {
				$bork = "hl=xx-bork&";
			}
		}
		$url =~ s/!bork!/$bork/;
	}

	return $url;
}
