use strict;
use warnings;
use diagnostics;

################################################################################
### goto_url_hook ##############################################################
sub goto_url_hook
{
	my $url = shift;
	my $current_url = shift;

	# "bugmenot" (no blood today, thank you)
	if ($url =~ '^bugmenot$' && $current_url)
	{
		(undef, $current_url) = $current_url =~ /^(.*):\/\/(.*)/;
		$url = 'http://bugmenot.com/view.php?url=' . $current_url;
		return $url;
	}

	# random URL generator
	if ($url =~ '^bored$' || $url =~ '^random$')
	{
		my $word; # you can say *that* again...
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

	# Google/Yahoo!/Ask Jeeves/A9/AltaVista/MSN/etcetera, search
	if ($url =~ '^((search|find|www|web|s|f)(| .*)|("|\'|`).*)$' ||
		$url =~ '^(eg|elgoog|hcraes|dnif|bew)(| .*)$' ||
		$url =~ '^(g|google)(| .*)$' ||
		$url =~ '^(y|yahoo)(| .*)$' ||
		$url =~ '^(ask|jeeves)(| .*)$' ||
		$url =~ '^a9(| .*)$' ||
		$url =~ '^(av|altavista)(| .*)$' ||
		$url =~ '^(msn|microsoft)(| .*)$')
	{
		my $engine = $url;
		my ($search) = $url =~ /^[a-z0-9]* (.*)/;
		$url = search(loadrc("search"), $search);
		$url = search("elgoog",         $search) if ($engine =~ '^(eg|elgoog|hcraes|dnif|bew)');
		$url = search("google",         $search) if ($engine =~ '^(g|google)');
		$url = search("yahoo",          $search) if ($engine =~ '^(y|yahoo)');
		$url = search("jeeves",         $search) if ($engine =~ '^(ask|jeeves)');
		$url = search("a9",             $search) if ($engine =~ '^a9');
		$url = search("altavista",      $search) if ($engine =~ '^(av|altavista)');
		$url = search("msn",            $search) if ($engine =~ '^(msn|microsoft)');
		$url = search(loadrc("search"), $engine) if ($engine =~ '^("|\'|`)');
		return $url;
	}

	# Google Groups (DejaNews)
	if ($url =~ '^(deja|gg|groups|gr|nntp|usenet)(| .*)$')
	{
		my ($search) = $url =~ /^[a-z]* (.*)/;
		my $beta = "groups.google.co.uk";
		$beta = "groups-beta.google.com" unless (loadrc("googlebeta") !~ 'yes');
		if (-f $ENV{"HOME"} . '/.elinks/beta')
		{
			$beta = "groups-beta.google.com";
		}
		my $bork = "";
		if ($search)
		{
			$bork = "&hl=xx-bork" unless (loadrc("bork") !~ 'yes');
			$url = 'http://' . $beta . '/groups?q=' . $search . $bork;
		}
		else
		{
			$bork = "/groups?hl=xx-bork" unless (loadrc("bork") !~ 'yes');
			$url = 'http://' . $beta . $bork;
		}
		return $url;
	}

	# MirrorDot
	if ($url =~ '^(mirrordot|md)(| .*)$')
	{
		my ($slashdotted) = $url =~ /^[a-z]* (.*)/;
		if ($slashdotted)
		{
			$url = 'http://mirrordot.com/find-mirror.html?' . $slashdotted;
		}
		else
		{
			$url = 'http://mirrordot.com';
		}
		return $url;
	}

	# the Bastard Operator from Hell
	if ($url =~ '^bofh$')
	{
		$url = 'http://prime-mover.cc.waikato.ac.nz/Bastard.html';
		return $url;
	}

	# Coral cache <URL>
	if ($url =~ '^(coral|cc|nyud)( .*)$')
	{
		my ($cache) = $url =~ /^[a-z]* (.*)/;
		$cache =~ s/^http:\/\///;
		($url) = $cache =~ s/\//.nyud.net:8090\//;
		$url = 'http://' . $cache;
		return $url;
	}

	# Babelfish ("babelfish german english"  or  "bf de en")
	if (($url =~ '^(babelfish|babel|bf|translate|trans|b)(| [a-zA-Z]* [a-zA-Z]*)$') ||
		($url =~ '^(babelfish|babel|bf|translate|trans|b)(| [a-zA-Z]*(| [a-zA-Z]*))$' &&
		(loadrc("language") !~ "no")) && $current_url)
	{
		if ($url =~ '^[a-z]*$')
		{
			$url = 'http://babelfish.altavista.com';
		}
		else
		{
			my $tongue = loadrc("language");
			$url = $url . " " . $tongue if ($tongue !~ "no" && $url !~ /^[a-z]* [a-zA-Z]* [a-zA-Z]*$/);
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
			(undef, $current_url) = $current_url =~ /^(.*):\/\/(.*)/;
			$url = 'http://babelfish.altavista.com/babelfish/urltrurl?lp=' . $from_language . '_' . $to_language . '&url=' . $current_url;
		}
		return $url;
	}

	# XYZZY
	if ($url =~ '^xyzzy$')
	{
		#$url = 'http://sundae.triumf.ca/pub2/cave/node001.html';
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
	if ($url =~ '^(news|n)(| .*)$' ||
		$url =~ '^bbc(| .*)$' ||
		$url =~ '^msnbc(| .*)$' ||
		$url =~ '^cnn(| .*)$' ||
		$url =~ '^fox(| .*)$' ||
		$url =~ '^gn(| .*)$' ||
		$url =~ '^yn(| .*)$' ||
		$url =~ '^(reuters|rs)(| .*)$' ||
		$url =~ '^eff(| .*)$' ||
		$url =~ '^(wired|wd)(| .*)$' ||
		$url =~ '^(\/\.|slashdot|sd)(| .*)$')
	{
		my ($search) = $url =~ /^.* (.*)/;
		my $news_bbc      = 'http://news.bbc.co.uk';  # British Broadcasting Corporation
		my $news_msnbc    = 'http://msnbc.com';       # the bastard child of Microsoft and the National Broadcasting Corporation
		my $news_cnn      = 'http://cnn.com';         # Cable News Network
		my $news_fox      = 'http://foxnews.com';     # FOX
		my $news_google   = 'http://news.google.com'; # Google
		my $news_yahoo    = 'http://news.yahoo.com';  # Yahoo!
		my $news_reuters  = 'http://reuters.com';     # Reuters
		my $news_eff      = 'http://eff.org';         # Electronic Frontier Foundation
		my $news_wired    = 'http://wired.com';       # Wired
		my $news_slashdot = 'http://slashdot.org';    # /.
		if ($search)
		{
			$news_bbc      = 'http://newssearch.bbc.co.uk/cgi-bin/search/results.pl?q=' . $search;
			$news_msnbc    = 'http://msnbc.msn.com/?id=3053419&action=fulltext&querytext=' . $search;
			$news_cnn      = 'http://search.cnn.com/pages/search.jsp?query=' . $search;
			$news_fox      = 'http://search.foxnews.com/info.foxnws/redirs_all.htm?pgtarg=wbsdogpile&qkw=' . $search;
			$news_google   = 'http://news.google.com/news?q=' . $search;
			$news_yahoo    = 'http://news.search.yahoo.com/search/news/?p=' . $search;
			$news_reuters  = 'http://reuters.com/newsSearchResultsHome.jhtml?query=' . $search;
			$news_eff      = 'http://google.com/custom?sitesearch=eff.org&q=' . $search;
			$news_wired    = 'http://search.wired.com/wnews/default.asp?query=' . $search;
			$news_slashdot = 'http://slashdot.org/search.pl?query=' . $search;
		}
		   if ($url =~ '^bbc')                { $url = $news_bbc; }
		elsif ($url =~ '^msnbc')              { $url = $news_msnbc; }
		elsif ($url =~ '^cnn')                { $url = $news_cnn; }
		elsif ($url =~ '^fox')                { $url = $news_fox; }
		elsif ($url =~ '^gn')                 { $url = $news_google; }
		elsif ($url =~ '^yn')                 { $url = $news_yahoo; }
		elsif ($url =~ '^(reuters|rs)')       { $url = $news_reuters; }
		elsif ($url =~ '^eff')                { $url = $news_eff; }
		elsif ($url =~ '^(wired|wd)')         { $url = $news_wired; }
		elsif ($url =~ '^(\/\.|slashdot|sd)') { $url = $news_slashdot; }
		else
		{
			$url = $news_bbc;
			if (loadrc("news") =~ 'bbc')             { $url = $news_bbc; }
			if (loadrc("news") =~ 'msnbc')           { $url = $news_msnbc; }
			if (loadrc("news") =~ 'cnn')             { $url = $news_cnn; }
			if (loadrc("news") =~ 'fox')             { $url = $news_fox; }
			if (loadrc("news") =~ 'google')          { $url = $news_google; }
			if (loadrc("news") =~ 'yahoo')           { $url = $news_yahoo; }
			if (loadrc("news") =~ 'reuters')         { $url = $news_reuters; }
			if (loadrc("news") =~ 'eff')             { $url = $news_eff; }
			if (loadrc("news") =~ 'wired')           { $url = $news_wired; }
			if (loadrc("news") =~ '(\/\.|slashdot)') { $url = $news_slashdot; }
		}
		return $url;
	}

	# and now, Deep Thoughts.  by Jack Handey
	if ($url =~ '^(jack|handey)$')
	{
		$url = 'http://glug.com/handey';
		return $url;
	}

	# locators
	if ($url =~ '^(imdb|movie|flick)(| .*)$' ||
		$url =~ '^zip(| .*)$' ||
		$url =~ '^ip(| .*)$' ||
		$url =~ '^whois(| .*)$' ||
		$url =~ '^rfc(| .*)$' ||
		$url =~ '^(weather|w)(| .*)$' ||
		$url =~ '^(stock|ticker|quote)(| .*)$' ||
		$url =~ '^(urban|legend|ul)(| .*)$' ||
		$url =~ '^(bittorrent|torrent|bt)(| .*)$')
	{
		my ($thingy) = $url =~ /^[a-z]* (.*)/;
		my $ipv = "4"; $ipv = "6" if loadrc('ipv6') =~ "yes";
		my $locator_imdb    = 'http://imdb.com';                                               # Internet Movie Database
		my $locator_zip     = 'http://usps.com';                                               # United States Postal Service
		my $locator_ip      = 'http://www.iana.org/assignments/ipv' . $ipv . '-address-space'; # Internet Protocol Version 4 address space
		my $locator_whois   = 'http://www.iana.org/cctld/cctld-whois.htm';                     # Internet Assigned Numbers Authority
		my $locator_rfc     = 'http://ietf.org';                                               # Internet Engineering Task Force
		my $locator_weather = 'http://weather.noaa.gov';                                       # National Oceanic and Atmospheric Administration
		my $locator_stock   = 'http://nasdr.com';                                              # NASD Regulation
		my $locator_bs      = 'http://snopes.com';                                             # Snopes (urban legends)
		my $locator_torrent = 'http://isohunt.com';                                            # ISO Hunt
		if ($thingy)
		{
			$locator_imdb        = 'http://imdb.com/Find?select=All&for=' . $thingy;                                # Internet Movie Database
			$locator_zip         = 'http://zip4.usps.com/zip4/zip_responseA.jsp?zipcode=' . $thingy;                # United States ZIP code search
			$locator_ip          = 'http://melissadata.com/lookups/iplocation.asp?ipaddress=' . $thingy;            # IP address locator
			$locator_whois       = 'http://reports.internic.net/cgi/whois?type=domain&whois_nic=' . $thingy;        # Internic WHOIS
			$locator_rfc         = 'http://rfc-editor.org/cgi-bin/rfcsearch.pl?searchwords=' . $thingy . '&num=25'; # Request for Comments
				$locator_rfc     = 'http://ietf.org/rfc/rfc' . $thingy . '.txt' unless ($thingy !~ '^[0-9]*$');
			my $weather          = loadrc('weather');
				$locator_weather = 'http://wunderground.com/cgi-bin/findweather/getForecast?query=' . $thingy;      # Weather Underground
				$locator_weather = 'http://google.com/search?q=weather+"' . $thingy . '"' if $weather =~ "google";
				$locator_weather = 'http://search.yahoo.com/search?p=weather+"' . $thingy . '"' if $weather =~ "yahoo";
				$locator_weather = 'http://weather.cnn.com/weather/search?wsearch=' . $thingy if $weather =~ "cnn";
				$locator_weather = 'http://wwwa.accuweather.com/adcbin/public/us_getcity.asp?zipcode=' . $thingy if $weather =~ "accuweather";
				$locator_weather = 'http://web.ask.com/web?&q=weather ' . $thingy if $weather =~ "(ask|jeeves|ask jeeves)";
			$locator_stock       = 'http://finance.yahoo.com/l?s=' . $thingy;                                       # Yahoo! Finance
			$locator_bs          = 'http://search.atomz.com/search/?sp-a=00062d45-sp00000000&sp-q=' . $thingy;      # urban legend search
			my $bork = ""; $bork = "&hl=xx-bork" unless (loadrc("bork") !~ 'yes');
				$locator_torrent = 'http://google.com/search?q=filetype:torrent ' . $thingy . $bork;                # torrent search
		}
		   if ($url =~ '^(imdb|movie|flick)')      { $url = $locator_imdb; }
		elsif ($url =~ '^zip')                     { $url = $locator_zip; }
		elsif ($url =~ '^ip')                      { $url = $locator_ip; }
		elsif ($url =~ '^whois')                   { $url = $locator_whois; }
		elsif ($url =~ '^rfc')                     { $url = $locator_rfc; }
		elsif ($url =~ '^(weather|w)')             { $url = $locator_weather; }
		elsif ($url =~ '^(stock|ticker|quote)')    { $url = $locator_stock; }
		elsif ($url =~ '^(urban|legend|ul)')       { $url = $locator_bs; }
		elsif ($url =~ '^(bittorrent|torrent|bt)') { $url = $locator_torrent; }
		return $url;
	}

################################################################################
### anything not otherwise useful is a search ##################################
#	if ($current_url) { $url = search(loadrc("search"), $url); }
	return $url;
}

################################################################################
### follow_url_hook ############################################################
sub follow_url_hook
{
	my $url = shift;

	# Bork! Bork! Bork!
	if ($url =~ 'google\.com')
	{
		if (loadrc("bork") =~ 'yes')
		{
			if ($url =~ '^http://(|www\.|search\.)google\.com(|/search)(|/)$')
			{
				$url = 'http://google.com/webhp?hl=xx-bork';
			}
			elsif ($url =~ '^http://(|www\.)groups\.google\.com(|/groups)(|/)$'
				|| $url =~ '^http://(|www\.|search\.)google\.com/groups(|/)$')
			{
				$url = 'http://google.com/groups?hl=xx-bork';
			}
		}
		return $url;
	}

	# nntp?  try Google Groups
	if ($url =~ '^(nntp|news):')
	{
		my $beta = "groups.google.co.uk";
		$beta = "groups-beta.google.com" unless (loadrc("googlebeta") !~ 'yes');
		if (-f $ENV{"HOME"} . '/.elinks/beta')
		{
			$beta = "groups-beta.google.com";
		}
		$url =~ s/\///g;
		my ($group) = $url =~ /[a-zA-Z]:(.*)/;
		my $bork = "";
		$bork = "hl=xx-bork&" unless (loadrc("bork") !~ 'yes');
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
	if ($url =~ 'slashdot\.org')
	{
#		$html =~ s/^<!-- Advertisement code. -->.*<!-- end ad code -->$//sm;
#		$html =~ s/<iframe.*><\/iframe>//g;
#		$html =~ s/<B>Advertisement<\/B>//;
	}

	# yes, I heard you the first time
	if ($url =~ 'google\.com')
	{
		$html =~ s/Teep: In must broosers yuoo cun joost heet zee retoorn key insteed ooff cleecking oon zee seerch boottun\. Bork bork bork!//;
		$html =~ s/Tip:<\/font> Save time by hitting the return key instead of clicking on "search"/<\/font>/;
	}

	# SourceForge ad smasher
	if ($url =~ 'sourceforge\.net')
	{
		$html =~ s/<!-- AD POSITION \d+ -->.*?<!-- END AD POSITION \d+ -->//smg;
		$html =~ s/<b>&nbsp\;&nbsp\;&nbsp\;Site Sponsors<\/b>//g;
	}

	# Gmail has obviously never met ELinks
	if ($url =~ 'gmail\.google\.com')
	{
		$html =~ s/^<b>For a better Gmail experience, use a.+?Learn more<\/a><\/b>$//sm;
	}


	# demoronizer
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
	if ($url =~ '^(file://|(http://|)(localhost|127\.0\.0\.1)(/|:|$))')
	{
		return "";
	}

	return;
}

################################################################################
### quit_hook ##################################################################
sub quit_hook
{
	# words of wisdom from ELinks the Sage
	if (loadrc("fortune") =~ "fortune")
	{
		system('echo ""; fortune -sa 2>/dev/null');
		die
	}
	die if (loadrc("fortune") =~ '^(none|quiet)$');
	my $cookiejar = 'elinks.fortune';
	my $ohwhynot = `ls /usr/share/doc/elinks*/$cookiejar 2>/dev/null`;
	open COOKIES, $ENV{"HOME"} . '/.elinks/' . $cookiejar
		or open COOKIES, '/etc/elinks/' . $cookiejar
		or open COOKIES, '/usr/share/elinks/' . $cookiejar
		or open COOKIES, $ohwhynot
		or die system('echo ""; fortune -sa 2>/dev/null');
	my (@line, $fortune);
	$line[0] = 0;
	while(<COOKIES>)
	{
		$line[$#line + 1] = tell if /^%$/;
	}
	srand();
	seek(COOKIES, $line[int rand($#line + 1)], 0);
	while(<COOKIES>)
	{
		last if /^%$/;
		$fortune .= $_;
	}
	close COOKIES;
	print("\n", $fortune);
}

################################################################################
### configuration ##############################################################
sub loadrc
{
	# # ~/.elinks/config.pl
	# bork:       yep		(BORKify Google?)
	# fortune:    fortune	(*fortune*, *elinks* tip, or *none* on quit?)
	# googlebeta: hell no	(I miss DejaNews.)
	# ipv6:       sure		(IPV4 or 6 address blocks with "ip" prefix?)
	# language:   english	("bf nl en" still works, but now "bf nl" does too)
	# news:       bbc		(agency to use for "news" and "n" prefixes)
	# search:     yahoo		(engine for (search|find|www|web|s|f) prefixes)
	# weather:    cnn		(server for "weather" and "w" prefixes)
	# # news: bbc, msnbc, cnn, fox, google, yahoo, reuters, eff, wired, and slashdot
	# # search: elgoog, google, yahoo, ask jeeves, a9, altavista, and msn
	# # weather: weather underground, google, yahoo, cnn, accuweather, ask jeeves
	my ($preference) = @_;
	my $configperl = $ENV{"HOME"} . '/.elinks/config.pl';
	if (-f $configperl)
	{
		open RC, "<$configperl";
		while(<RC>)
		{
			next if (m/^\#.*/);
			next if (!m/(.*):\s*(.*)/);
			my $setting = $1;
			my $switch = $2;
			if ($setting =~ $preference)
			{
				if ($switch =~ '^(yes|1|on|yea|yep|sure|ok|okay|yeah)$')
				{
					return "yes";
				}
				elsif ($switch =~ '^(no|0|off|nay|nope|nah|hell no)$')
				{
					return "no";
				}
				else
				{
					return lc($switch);
				}
			}
		}
		close RC;
	}
	return "no";
}

################################################################################
### search engines #############################################################
sub search
{
	my ($engine, $search) = @_;
	my $url;
	my $bork = "";
	if ($engine =~ '^elgoog')
	{
		$url = 'http://alltooflat.com/geeky/elgoog/m/index.cgi';
		$url = 'http://alltooflat.com/geeky/elgoog/m/index.cgi?page=%2fsearch&cgi=get&q=' . $search if $search;
	}
	elsif ($engine =~ '^google')
	{
		$bork = "/webhp?hl=xx-bork" unless (loadrc("bork") !~ 'yes');
		$url = 'http://google.com' . $bork;
		$bork = "hl=xx-bork&" unless (loadrc("bork") !~ 'yes');
		$url = 'http://google.com/search?' . $bork . 'q=' . $search if $search;
	}
	elsif ($engine =~ '^yahoo')
	{
		$url = 'http://yahoo.com';
		$url = 'http://search.yahoo.com/search?p=' . $search if $search;
	}
	elsif ($engine =~ '^(ask|jeeves|ask jeeves)')
	{
		$url = 'http://ask.com';
		$url = 'http://web.ask.com/web?q=' . $search if $search;
	}
	elsif ($engine =~ '^a9')
	{
		$url = 'http://a9.com';
		$url = 'http://a9.com/?q=' . $search if $search;
	}
	elsif ($engine =~ '^(altavista|av)')
	{
		$url = 'http://altavista.com';
		$url = 'http://altavista.com/web/results?q=' . $search if $search;
	}
	elsif ($engine =~ '^(msn|microsoft)')
	{
		$url = 'http://msn.com';
		$url = 'http://search.msn.com/results.aspx?q=' . $search if $search;
	}
	else # default
	{
		$bork = "/webhp?hl=xx-bork" unless (loadrc("bork") !~ 'yes');
		$url = 'http://google.com' . $bork;
		$bork = "hl=xx-bork&" unless (loadrc("bork") !~ 'yes');
		$url = 'http://google.com/search?' . $bork . 'q=' . $search if $search;
	}
	return $url;
}


# vim: ts=4 sw=4 sts=0
