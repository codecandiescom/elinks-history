use strict;
use warnings;
use diagnostics;

################################################################################
### goto_url_hook ##############################################################
sub goto_url_hook
{
	my $url = shift;
	my $current_url = shift;

	# "bugmenot" anti-soul-sucker
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

	# Google/Yahoo!/Ask Jeeves/A9/AltaVista/MSN, search
	if ($url =~ '^(g|google|search|find)(| .*)$' ||
		$url =~ '^(y|yahoo)(| .*)$' ||
		$url =~ '^(ask|jeeves)(| .*)$' ||
		$url =~ '^a9(| .*)$' ||
		$url =~ '^(av|altavista)(| .*)$' ||
		$url =~ '^msn(| .*)$')
	{
		my ($search) = $url =~ /^[a-z0-9]* (.*)/;
		my $bork = "";
		$bork = "/webhp?hl=xx-bork" unless (loadrc("bork") !~ 'yes');
		my $search_google    = 'http://google.com' . $bork;
		my $search_yahoo     = 'http://yahoo.com';
		my $search_jeeves    = 'http://ask.com';
		my $search_a9        = 'http://a9.com';
		my $search_altavista = 'http://altavista.com';
		my $search_msn       = 'http://msn.com';
		if ($search)
		{
			$bork = "hl=xx-bork&" unless (loadrc("bork") !~ 'yes');
			$search_google    = 'http://google.com/search?' . $bork . 'q=' . $search;
			$search_yahoo     = 'http://search.yahoo.com/search?p=' . $search;
			$search_jeeves    = 'http://web.ask.com/web?q=' . $search;
			$search_a9        = 'http://a9.com/?q=' . $search;
			$search_altavista = 'http://altavista.com/web/results?q=' . $search;
			$search_msn       = 'http://search.msn.com/results.aspx?q=' . $search;
		}
		   if ($url =~ '^(y|yahoo)')      { $url = $search_yahoo; }
		elsif ($url =~ '^(ask|jeeves)')   { $url = $search_jeeves; }
		elsif ($url =~ '^a9')             { $url = $search_a9; }
		elsif ($url =~ '^(av|altavista)') { $url = $search_altavista; }
		elsif ($url =~ '^msn')            { $url = $search_msn; }
		else                              { $url = $search_google; }
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

	# /.
	if ($url =~ '^((\/\.|slashdot|sd)|(mirrordot|md))(| .*)$')
	{
		my ($slashdotted) = $url =~ /^.* (.*)/;
		if ($url !~ '^m')
		{
			if ($slashdotted)
			{
				$url = 'http://slashdot.org/search.pl?query=' . $slashdotted;
			}
			else
			{
				$url = 'http://slashdot.org';
			}
		}
		else
		{
			if ($slashdotted)
			{
				$url = 'http://mirrordot.com/find-mirror.html?' . $slashdotted;
			}
			else
			{
				$url = 'http://mirrordot.com';
			}
		}
		return $url;
	}

	# xiferp trams hcraes elgooG
	if ($url =~ '^(eg|elgoog|hcraes|dnif)(| .*)$')
	{
		my ($hcraes) = $url =~ /^[a-z]* (.*)/;
		if ($hcraes)
		{
			$url = 'http://alltooflat.com/geeky/elgoog/m/index.cgi?page=%2fsearch&cgi=get&q=' . $hcraes;
		}
		else
		{
			$url = 'http://alltooflat.com/geeky/elgoog/m/index.cgi';
		}
		return $url;
	}

	# the Bastard Operator from Hell
	if ($url =~ '^bofh$')
	{
		$url = 'http://prime-mover.cc.waikato.ac.nz/Bastard.html';
		return $url;
	}

	# torrent search
	if ($url =~ '^(bittorrent|torrent|bt)( .*)$')
	{
		my ($torrent) = $url =~ /^[a-z]* (.*)/;
		my $bork = "";
		$bork = "&hl=xx-bork" unless (loadrc("bork") !~ 'yes');
		$url = 'http://google.com/search?q=filetype:torrent ' . $torrent . $bork;
		return $url;
	}

	# Coral cache
	if ($url =~ '^(coral|cc|nyud)( .*)$')
	{
		my ($cache) = $url =~ /^[a-z]* (.*)/;
		$cache =~ s/^http:\/\///;
		($url) = $cache =~ s/\//.nyud.net:8090\//;
		$url = 'http://' . $cache;
		return $url;
	}

	# Babelfish ("babelfish german english")
	if ($url =~ '^(babelfish|babel|bf|translate|trans|b)(| [a-zA-Z]* [a-zA-Z]*)$' && $current_url)
	{
		if ($url =~ '^[a-z]*$')
		{
			$url = 'http://babelfish.altavista.com';
		}
		else
		{
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
		my $xyzzy = int(rand(6));
		   if ($xyzzy == 0) {$xyzzy = 1;}   # Colossal Cave Adventure
		elsif ($xyzzy == 1) {$xyzzy = 227;} # Zork Zero: The Revenge of Megaboz    (883 G.U.E.)
		elsif ($xyzzy == 2) {$xyzzy = 3;}   # Zork I: The Great Underground Empire (948 G.U.E.)
		elsif ($xyzzy == 3) {$xyzzy = 4;}   # Zork II: The Wizard of Frobozz       (948 G.U.E.)
		elsif ($xyzzy == 4) {$xyzzy = 5;}   # Zork III: The Dungeon Master         (948 G.U.E.)
		elsif ($xyzzy == 5) {$xyzzy = 6;}   # Zork: The Undiscovered Underground   (1066 G.U.E.)
		$url = 'http://ifiction.org/games/play.php?game=' . $xyzzy;
		return $url;
	}

	# News
	if ($url =~ '^(news|reuters|bbc|msnbc|cnn|fox|gn|yn)(| .*)$')
	{
		my ($search) = $url =~ /^[a-z]* (.*)/;
		my $news_reuters = 'http://reuters.com';     # Reuters
		my $news_bbc     = 'http://news.bbc.co.uk';  # British Broadcasting Corporation
		my $news_msnbc   = 'http://msnbc.com';       # the bastard child of Microsoft and the National Broadcasting Corporation
		my $news_cnn     = 'http://cnn.com';         # Cable News Network
		my $news_fox     = 'http://foxnews.com';     # FOX
		my $news_google  = 'http://news.google.com'; # Google
		my $news_yahoo   = 'http://news.yahoo.com';  # Yahoo!
		if ($search)
		{
			$news_reuters = 'http://reuters.com/newsSearchResultsHome.jhtml?query=' . $search;
			$news_bbc     = 'http://newssearch.bbc.co.uk/cgi-bin/search/results.pl?q=' . $search;
			$news_msnbc   = 'http://msnbc.msn.com/?id=3053419&action=fulltext&querytext=' . $search;
			$news_cnn     = 'http://search.cnn.com/pages/search.jsp?query=' . $search;
			$news_fox     = 'http://search.foxnews.com/info.foxnws/redirs_all.htm?pgtarg=wbsdogpile&qkw=' . $search;
			$news_google  = 'http://news.google.com/news?q=' . $search;
			$news_yahoo   = 'http://news.search.yahoo.com/search/news/?p=' . $search;
		}
		   if ($url =~ '^bbc')   { $url = $news_bbc; }
		elsif ($url =~ '^msnbc') { $url = $news_msnbc; }
		elsif ($url =~ '^cnn')   { $url = $news_cnn; }
		elsif ($url =~ '^fox')   { $url = $news_fox; }
		elsif ($url =~ '^gn')    { $url = $news_google; }
		elsif ($url =~ '^yn')    { $url = $news_yahoo; }
		else                     { $url = $news_reuters; }
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
		$url =~ '^(weather|w)(| .*)$' ||
		$url =~ '^rfc(| .*)$' ||
		$url =~ '^(stock|ticker|quote)(| .*)$' ||
		$url =~ '^(urban|legend|ul)(| .*)$')
	{
		my ($thingy) = $url =~ /^[a-z]* (.*)/;
		my $locator_imdb    = 'http://imdb.com';                                    # Internet Movie Database
		my $locator_zip     = 'http://usps.com';                                    # United States Postal Service
		my $locator_ip      = 'http://www.iana.org/assignments/ipv4-address-space';
		#my $locator_ip     = 'http://www.iana.org/assignments/ipv6-address-space';
		my $locator_whois   = 'http://www.iana.org/cctld/cctld-whois.htm';          # Internet Assigned Numbers Authority
		my $locator_weather = 'http://weather.noaa.gov';                            # National Oceanic and Atmospheric Administration
		my $locator_rfc     = 'http://ietf.org';                                    # Internet Engineering Task Force
		my $locator_stock   = 'http://nasdr.com';                                   # NASD Regulation
		my $locator_bs      = 'http://snopes.com';									# Snopes (urban legends)
		if ($thingy)
		{
			$locator_imdb  = 'http://imdb.com/Find?select=All&for=' . $thingy;                         # Internet Movie Database
			$locator_zip   = 'http://zip4.usps.com/zip4/zip_responseA.jsp?zipcode=' . $thingy;         # United States ZIP code search
			$locator_ip    = 'http://melissadata.com/lookups/iplocation.asp?ipaddress=' . $thingy;     # IP address locator
			$locator_whois = 'http://reports.internic.net/cgi/whois?type=domain&whois_nic=' . $thingy; # Internic WHOIS
			my $weather = "google";
			$weather = "yahoo" unless (loadrc('weather') !~ "yahoo");
			if ($weather =~ "yahoo")
			{
				$locator_weather = 'http://search.yahoo.com/search?p=weather+"' . $thingy . '"'; # Yahoo weather
			}
			else
			{
				$locator_weather = 'http://google.com/search?q=weather+"' . $thingy . '"';       # Google weather
			}
			if ($thingy =~ '^[0-9]*$')
			{
				$locator_rfc   = 'http://ietf.org/rfc/rfc' . $thingy . '.txt';                   # Request for Comments
			}
			else
			{
				$locator_rfc   = 'http://rfc-editor.org/cgi-bin/rfcsearch.pl?searchwords=' . $thingy . '&num=25';
			}
			$locator_stock = 'http://finance.yahoo.com/l?s=' . $thingy;                                  # Yahoo! Finance
			$locator_bs    = 'http://search.atomz.com/search/?sp-a=00062d45-sp00000000&sp-q=' . $thingy; # urban legend search
		}
		   if ($url =~ '^(imdb|movie|flick)')   { $url = $locator_imdb; }
		elsif ($url =~ '^zip')                  { $url = $locator_zip; }
		elsif ($url =~ '^ip')                   { $url = $locator_ip; }
		elsif ($url =~ '^whois')                { $url = $locator_whois; }
		elsif ($url =~ '^(weather|w)')          { $url = $locator_weather; }
		elsif ($url =~ '^rfc')                  { $url = $locator_rfc; }
		elsif ($url =~ '^(stock|ticker|quote)') { $url = $locator_stock; }
		elsif ($url =~ '^(urban|legend|ul)')    { $url = $locator_bs; }
		return $url;
	}

################################################################################
### anything not otherwise useful is a search ##################################
#	if ($current_url)
#	{
#		my $engine = loadrc("search");
#		   if ($engine =~ 'yahoo')           { $url = 'http://search.yahoo.com/search?p=' . $url; }
#		elsif ($engine =~ '(ask|jeeves)')    { $url = 'http://web.ask.com/web?q=' . $url; }
#		elsif ($engine =~ 'a9')              { $url = 'http://a9.com/?q=' . $url; }
#		elsif ($engine =~ 'altavista')       { $url = 'http://altavista.com/web/results?q=' . $url; }
#		elsif ($engine =~ '(msn|microsoft)') { $url = 'http://search.msn.com/results.aspx?q=' . $url; }
#		else
#		{
#			my $bork = "";
#			$bork = "&hl=xx-bork" unless (loadrc("bork") !~ 'yes');
#			$url = 'http://google.com/search?q=' . $url . $bork;
#		}
#	}
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
		my $beta = "groups.google.co.uk"; # You lika de beta?  I no lika de beta.
		$beta = "groups-beta.google.com" unless (loadrc("googlebeta") !~ 'yes');
		if (-f $ENV{"HOME"} . '/.elinks/beta')
		{
			$beta = "groups-beta.google.com"; # I like ugly sphincterfaces.
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
	# bork:       yep
	# googlebeta: hell no
	# search:     google
	# weather:    google
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
				if ($switch =~ '(yes|1|on|yea|yep)')
				{
					return "yes";
				}
				elsif ($switch =~ '(no|0|off|nay|nope)')
				{
					return "no";
				}
				else
				{
					return $switch;
				}
			}
		}
		close RC;
	}
	return "no";
}
