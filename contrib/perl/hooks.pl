# Example hooks.pl file, put in ~/.elinks/ as hooks.pl.
# $Id: hooks.pl,v 1.60 2005/03/27 11:26:23 pasky Exp $
#
# This file is (c) Russ Rowan and Petr Baudis and GPL'd.
#
# To get the complete documentation for this file in a user-friendly
# manner, do either
#	pod2html hooks.pl > hooks.html && elinks hooks.html
# or
#	perldoc hooks.pl


=head1 NAME

hooks.pl -- the Perl hooks for the ELinks text WWW browser

=head1 DESCRIPTION

This file contains the Perl hoks for the ELinks text WWW browser.

These hooks can alter the browser's behaviour in various ways; probably the
most popular is processing the user input in the Goto URL dialog and rewriting
it in various ways (like the builtin URL prefixes capability, but much more
powerful and flexible).

Other popular capability is the hooks is to rewrite the HTML source of the page
before it is rendered, usually to get rid of the ads and/or make the web page
more ELinks-friendly. The hooks also allow you to fine-tune the proxying rules,
can show a fortune when ELinks exits, and more!

=cut


use strict;
use warnings;
use diagnostics;

use Carp;


=head1 CONFIGURATION FILE

The hooks file reads its file from I<~/.elinks/config.pl>.
Note that the following is only an example,
and does not contain the default values:

	bork:       yep       # BORKify Google?
	collapse:   okay      # Collapse all XBEL bookmark folders on exit?
	fortune:    elinks    # *fortune*, *elinks* tip, or *none* on quit?
	googlebeta: hell no   # I miss DejaNews...
	gotosearch: not yet   # Don't use this yet.  It's broken.
	ipv6:       sure      # IPV4 or 6 address blocks with "ip" prefix?
	language:   english   # "bf nl en" still works, but now "bf nl" does too
	news:       msnbc     # Agency to use for "news" and "n" prefixes
	search:     elgoog    # Engine for (search|find|www|web|s|f|go) prefixes
	usenet:     google    # *google* or *standard* view for news:// URLs
	weather:    cnn       # Server for "weather" and "w" prefixes

	# news:    bbc, msnbc, cnn, fox, google, yahoo, reuters, eff, wired,
	#          slashdot, newsforge, usnews, newsci, discover, sciam
	# search:  elgoog, google, yahoo, ask jeeves, a9, altavista, msn, dmoz,
	#          dogpile, mamma, webcrawler, netscape, lycos, hotbot, excite
	# weather: weather underground, google, yahoo, cnn, accuweather,
	#          ask jeeves

=cut

=head1 PREFIXES

Don't call the prefixes "dumb", they hate that!  Rather, "interactivity
challenged".  (Such politically correct names always appeared to me to be
so much more insulting... --pasky ;-)

=head2 Misc. prefixes

B<bugmenot>, B<bored>, B<random>, B<bofh>, B<xyzzy>, B<jack> or B<handey>

=over 4

=item Google Groups

B<deja>, B<gg>, B<groups>, B<gr>, B<nntp>, B<usenet>, B<nn>

=item AltaVista Babelfish

B<babelfish>, B<babel>, B<bf>, B<translate>, B<trans>, or B<b>

=item MirrorDot

B<md> or B<mirrordot>

=item Coral cache

B<cc>, B<coral>, or B<nyud> (requires URL)

=item W3C page validators

B<vhtml> or B<vcss> (current url or specified)

=item The Dialectizer

B<dia> <dialect> (current url or specified)

Dialects can be:
I<redneck>, I<jive>, I<cockney>, I<fudd>, I<bork>, I<moron>, I<piglatin>, or I<hacker>

=back


=head2 Web search

=cut

#######################################################################

my %search_prefixes = (

=over 4

=item Default engine

B<search>, B<find>, B<www>, B<web>, B<s>, B<f>, B<go>,
and anything in quotes with no prefix.

=item Google

B<g> or B<google>

=cut
	'^(g|google)(| .*)$' => 'google',

=item Yahoo

B<y> or B<yahoo>

=cut
	'^(y|yahoo)(| .*)$' => 'yahoo',

=item Ask Jeeves

B<ask> or B<jeeves>

=cut
	'^(ask|jeeves)(| .*)$' => 'ask jeeves',

=item Amazon/A9

B<a9>

=cut
	'^a9(| .*)$' => 'a9',

=item Altavista

B<av> or B<altavista>

=cut
	'^(av|altavista)(| .*)$' => 'altavista',

=item Microsoft

B<msn> or B<microsoft>

=cut
	'^(msn|microsoft)(| .*)$' => 'msn',

=item dmoz

B<dmoz>, B<odp>, B<mozilla>

=cut
	'^(dmoz|odp|mozilla)(| .*)$' => 'dmoz',

=item Dogpile

B<dp> or B<dogpile>

=cut
	'^(dp|dogpile)(| .*)$' => 'dogpile',

=item Mamma

B<ma> or B<mamma>

=cut
	'^(ma|mamma)(| .*)$' => 'mamma',

=item Webcrawler

B<wc> or B<webcrawler>

=cut
	'^(wc|webcrawler)(| .*)$' => 'webcrawler',

=item Netscape

B<ns> or B<netscape>

=cut
	'^(ns|netscape)(| .*)$' => 'netscape',

=item Lycos

B<ly> or B<lycos>

=cut
	'^(ly|lycos)(| .*)$' => 'lycos',

=item Hotbot

B<hb> or B<hotbot>

=cut
	'^(hb|hotbot)(| .*)$' => 'hotbot',

=item Excite

B<ex> or B<excite>

=cut
	'^(ex|excite)(| .*)$' => 'excite',

=item Elgoog

B<eg>, B<elgoog>, B<hcraes>, B<dnif>, B<bew>, B<og>

=cut
	'^(eg|elgoog|hcraes|dnif|bew|og)(| .*)$' => 'elgoog',
);

#######################################################################

=back



=head2 News agencies

=cut

my %news_prefixes = (

=over 4

=item Default agency

B<n>, B<news>

=item British Broadcasting Corporation

B<bbc>

=cut
	'^bbc(| .*)$' => 'bbc',

=item MSNBC

B<msnbc>

=cut
	'^msnbc(| .*)$' => 'msnbc',

=item Cable News Network

B<cnn>

=cut
	'^cnn(| .*)$' => 'cnn',

=item FOXNews

B<fox>

=cut
	'^fox(| .*)$' => 'fox',

=item Google News

B<gn>

=cut
	'^gn(| .*)$' => 'google',

=item Yahoo News

B<yn>

=cut
	'^yn(| .*)$' => 'yahoo',

=item Reuters

B<rs> or B<reuters>

=cut
	'^(reuters|rs)(| .*)$' => 'reuters',

=item Electronic Frontier Foundation

B<eff>

=cut
	'^eff(| .*)$' => 'eff',

=item Wired

B<wd> or B<wired>

=cut
	'^(wired|wd)(| .*)$' => 'wired',

=item Slashdot

B</.> or B<sd> or B<slashdot>

=cut
	'^(\/\.|slashdot|sd)(| .*)$' => 'slashdot',

=item NewsForge

B<nf> or B<newsforge>

=cut
	'^(newsforge|nf)(| .*)$' => 'newsforge',

=item U.S.News & World Report

B<us> or B<usnews>

=cut
	'^(us|usnews)(| .*)$' => 'usnews',

=item New Scientist

B<newsci> or B<nsci>

=cut
	'^(nsci|newsci)(| .*)$' => 'newsci',

=item Discover Magazine

B<dm>

=cut
	'^dm(| .*)$' => 'discover',

=item Scientific American

B<sa> or B<sciam>

=cut
	'^(sa|sciam)(| .*)$' => 'sciam',
);

#######################################################################

=back


=head2 Locators

=cut

# Some of those are handled specially and not in this hash.
my %locator_prefixes = (

=over 4

=item Internet Movie Database

B<imdb>, B<movie>, or B<flick>

=cut
	'^(imdb|movie|flick)(| .*)$' => 'imdb',

=item US zip code search

B<zip> or B<usps> (# or address)

=cut

=item IP address locator / address space

B<ip>

=cut

=item WHOIS / TLD list

B<whois> (current url or specified)

=cut

=item Request for Comments

B<rfc> (# or search)

=cut

=item Weather

B<w> or B<weather>

=cut

=item Yahoo! Finance / NASD Regulation

B<stock>, B<ticker>, or B<quote>

=cut
	'^(stock|ticker|quote)(| .*)$' => 'stock',

=item Snopes

B<ul>, B<urban>, or B<legend>

=cut
	'^(urban|legend|ul)(| .*)$' => 'bs',

=item Torrent search / ISOHunt

B<bt>, B<torrent>, or B<bittorrent>

=cut
	'^(bittorrent|torrent|bt)(| .*)$' => 'torrent',

=item Wayback Machine

B<ia>, B<ar>, B<arc>, or B<archive> (current url or specified)

=cut
	'^(archive|arc|ar|ia)(| .*)$' => 'archive',

=item Freshmeat

B<fm> or B<freshmeat>

=cut
	'^(freshmeat|fm)(| .*)$' => 'freshmeat',

=item SourceForge

B<sf> or B<sourceforge>

=cut
	'^(sourceforge|sf)(| .*)$' => 'sourceforge',

=item Savannah

B<sv> or B<savannah>

=cut
	'^(savannah|sv)(| .*)$' => 'savannah',

=item Gna!

B<gna>

=cut
	'^gna(| .*)$' => 'gna',

=item Netcraft Uptime Survey

B<whatis> or B<uptime> (current url or specified)

=cut

=item Who's Alive and Who's Dead

Wanted, B<dead> or B<alive>!

=cut
	'^(alive|dead)(| .*)$' => 'dead',

=item Google Library / Project Gutenberg

B<book> or B<read>

=cut
	'^(book|read)(| .*)$' => 'book',

=item Internet Public Library

B<ipl>

=cut
	'^ipl(| .*)$' => 'ipl',
);

#######################################################################

=back

=head2 ELinks

=over 4

=item Home

B<el> or B<elinks>

=item Bugzilla

B<bz> or B<bug> (# or search optional)

=item Documentation and FAQ

B<doc(|s|umentation)> or B<faq>

=back

=cut


my %weather_locators = (
	'weather underground' => 'http://wunderground.com/cgi-bin/findweather/getForecast?query=!query!',
	'google' => 'http://google.com/search?q=weather+"!query!"',
	'yahoo' => 'http://search.yahoo.com/search?p=weather+"!query!"',
	'cnn' => 'http://weather.cnn.com/weather/search?wsearch=!query!',
	'accuweather' => 'http://wwwa.accuweather.com/adcbin/public/us_getcity.asp?zipcode=!query!',
	'ask jeeves' => 'http://web.ask.com/web?&q=weather !query!',
);


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
		return 'http://bugmenot.com/view.php?url=' . $current_url;
	}

	# Random URL generator
	if ($url =~ '^bored$' || $url =~ '^random$')
	{
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
		return 'http://' . lc($word) . '.com';
	}

	# Search engines
	my ($search) = $url =~ /^\S+\s+(.*)/;
	if ($url =~ /^(search|find|www|web|s|f|go)(| .*)$/)
	{
		return search(loadrc('search'), $search);
	}
	if ($url =~ s/^("|\'|')(.+)$/$2/)
	{
		return search(loadrc('search'), $url);
	}
	foreach my $prefix (keys %search_prefixes)
	{
		next unless $url =~ /$prefix/;
		return search($search_prefixes{$prefix}, $search);
	}

	# News
	if ($url =~ /^(news|n)(| .*)$/)
	{
		return news(loadrc('news'), $search);
	}
	foreach my $prefix (keys %news_prefixes)
	{
		next unless $url =~ /$prefix/;
		return news($news_prefixes{$prefix}, $search);
	}

	# Locators
	foreach my $prefix (keys %locator_prefixes)
	{
		next unless $url =~ /$prefix/;
		return location($locator_prefixes{$prefix}, $search, $current_url);
	}

	if ($url =~ '^(zip|usps)(| .*)$'
		or $url =~ '^ip(| .*)$'
		or $url =~ '^whois(| .*)$'
		or $url =~ '^rfc(| .*)$'
		or $url =~ '^(weather|w)(| .*)$'
		or $url =~ '^(whatis|uptime)(| .*)$') {
		my ($thingy) = $url =~ /^[a-z]* (.*)/;
		my ($domain) = $current_url =~ /([a-z0-9-]+\.(com|net|org|edu|gov|mil))/;

		my $locator_zip            = 'http://usps.com';
		my $ipv                    = "ipv4-address-space"; $ipv = "ipv6-address-space" if loadrc("ipv6") eq "yes";
			my $locator_ip         = 'http://www.iana.org/assignments/' . $ipv;
		my $whois                  = 'http://reports.internic.net/cgi/whois?type=domain&whois_nic=';
			my $locator_whois      = 'http://www.iana.org/cctld/cctld-whois.htm';
			$locator_whois         = $whois . $domain if $domain;
		my $locator_rfc            = 'http://ietf.org';
		my $locator_weather        = 'http://weather.noaa.gov';
		my $locator_whatis         = 'http://uptime.netcraft.com';
			$locator_whatis        = 'http://uptime.netcraft.com/up/graph/?host=' . $domain if $domain;

		if ($thingy)
		{
			$locator_zip           = 'http://zip4.usps.com/zip4/zip_responseA.jsp?zipcode=' . $thingy;
				$locator_zip       = 'http://zipinfo.com/cgi-local/zipsrch.exe?zip=' . $thingy if $thingy !~ '^[0-9]*$';
			$locator_ip            = 'http://melissadata.com/lookups/iplocation.asp?ipaddress=' . $thingy;
			$locator_whois         = $whois . $thingy;
			$locator_rfc           = 'http://rfc-editor.org/cgi-bin/rfcsearch.pl?num=37&searchwords=' . $thingy;
				$locator_rfc       = 'http://ietf.org/rfc/rfc' . $thingy . '.txt' unless $thingy !~ '^[0-9]*$';
			my $weather            = loadrc("weather");
				$locator_weather   = $weather_locators{$weather};
				$locator_weather ||= $weather_locators{'weather underground'};
				$locator_weather   =~ s/!query!/$thingy/;
			$locator_whatis        = 'http://uptime.netcraft.com/up/graph/?host=' . $thingy;
		}
		return $locator_zip         if ($url =~ '^(zip|usps)(| .*)$');
		return $locator_ip          if ($url =~ '^ip(| .*)$');
		return $locator_whois       if ($url =~ '^whois(| .*)$');
		return $locator_rfc         if ($url =~ '^rfc(| .*)$');
		return $locator_weather     if ($url =~ '^(weather|w)(| .*)$');
		return $locator_whatis      if ($url =~ '^(whatis|uptime)(| .*)$');
	}

	# Google Groups (DejaNews)
	if ($url =~ '^(deja|gg|groups|gr|nntp|usenet|nn)(| .*)$')
	{
		my ($search) = $url =~ /^[a-z]* (.*)/;
		my $beta = "groups.google.co.uk";
		$beta = "groups-beta.google.com" unless (loadrc("googlebeta") ne "yes");
		my $bork = "";
		if ($search)
		{
			$bork = "&hl=xx-bork" unless (loadrc("bork") ne "yes");
			return 'http://' . $beta . '/groups?q=' . $search . $bork;
		}
		else
		{
			$bork = "/groups?hl=xx-bork" unless (loadrc("bork") ne "yes");
			return 'http://' . $beta . $bork;
		}
	}

	# MirrorDot
	if ($url =~ '^(mirrordot|md)(| .*)$')
	{
		my ($slashdotted) = $url =~ /^[a-z]* (.*)/;
		if ($slashdotted)
		{
			return 'http://mirrordot.com/find-mirror.html?' . $slashdotted;
		}
		else
		{
			return 'http://mirrordot.com';
		}
	}

	# The Bastard Operator from Hell
	if ($url =~ '^bofh$')
	{
		return 'http://prime-mover.cc.waikato.ac.nz/Bastard.html';
	}

	# Coral cache <URL>
	if ($url =~ '^(coral|cc|nyud)( .*)$')
	{
		my ($cache) = $url =~ /^[a-z]* (.*)/;
		$cache =~ s/^http:\/\///;
		($url) = $cache =~ s/\//.nyud.net:8090\//;
		return 'http://' . $cache;
	}

	# Babelfish ("babelfish german english"  or  "bf de en")
	if (($url =~ '^(babelfish|babel|bf|translate|trans|b)(| [a-zA-Z]* [a-zA-Z]*)$')
		or ($url =~ '^(babelfish|babel|bf|translate|trans|b)(| [a-zA-Z]*(| [a-zA-Z]*))$'
		and loadrc("language") and $current_url))
	{
		$url = 'http://babelfish.altavista.com' if ($url =~ /^[a-z]*$/);
		if ($url =~ /^[a-z]* /)
		{
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
	if ($url =~ '^xyzzy$')
	{
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
		return 'http://ifiction.org/games/play.php?game=' . $yzzyx;
	}

	# ...and now, Deep Thoughts.  by Jack Handey
	if ($url =~ '^(jack|handey)$')
	{
		return 'http://glug.com/handey';
	}

	# Page validators [<URL>]
	if ($url =~ '^vhtml(| .*)$' or $url =~ '^vcss(| .*)$')
	{
		my ($page) = $url =~ /^.* (.*)/;
		$page = $current_url unless $page;
		return 'http://validator.w3.org/check?uri=' . $page if $url =~ 'html';
		return 'http://jigsaw.w3.org/css-validator/validator?uri=' . $page if $url =~ 'css';
	}

	# There's no place like home
	if ($url =~ '^(el(|inks)|b(ug(|s)|z)(| .*)|doc(|umentation|s)|faq)$')
	{
		my ($bug) = $url =~ /^.* (.*)/;
		if ($url =~ '^b')
		{
			my $bugzilla = 'http://bugzilla.elinks.or.cz';
			if (not $bug)
			{
				return $bugzilla;
			}
			elsif ($bug =~ '^[0-9]*$')
			{
				return $bugzilla . '/show_bug.cgi?id=' . $bug;
			}
			else
			{
				return $bugzilla . '/buglist.cgi?short_desc_type=allwordssubstr&short_desc=' . $bug;
			}
		}
		else
		{
			my $doc = '';
			$doc = '/documentation' if $url =~ '^doc';
			$doc = '/faq.html' if $url =~ '^faq$';
			return 'http://elinks.or.cz' . $doc;
		}
	}

	# the Dialectizer (dia <dialect> <url>)
	if ($url =~ '^dia(| [a-z]*(| .*))$')
	{
		my ($dialect) = $url =~ /^dia ([a-z]*)/;
			$dialect = "hckr" if $dialect and $dialect eq 'hacker';
		my ($victim) = $url =~ /^dia [a-z]* (.*)$/;
			$victim = $current_url if (!$victim and $current_url and $dialect);
		$url = 'http://rinkworks.com/dialect';
		if ($dialect and $dialect =~ '^(redneck|jive|cockney|fudd|bork|moron|piglatin|hckr)$' and $victim)
		{
			$victim =~ s/^http:\/\///;
			$url = $url . '/dialectp.cgi?dialect=' . $dialect . '&url=http%3a%2f%2f' . $victim . '&inside=1';
		}
		return $url;
	}


	# Anything not otherwise useful could be a search
	if ($current_url and loadrc("gotosearch") eq "yes")
	{
		return search(loadrc("search"), $url);
	}

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
		if (loadrc("bork") eq "yes")
		{
			if ($url =~ '^http://(|www\.|search\.)google\.com(|/search)(|/)$')
			{
				return 'http://google.com/webhp?hl=xx-bork';
			}
			elsif ($url =~ '^http://(|www\.)groups\.google\.com(|/groups)(|/)$'
				or $url =~ '^http://(|www\.|search\.)google\.com/groups(|/)$')
			{
				return 'http://google.com/groups?hl=xx-bork';
			}
		}
	}

	# NNTP?  Try Google Groups
	if ($url =~ '^(nntp|news):' and loadrc("usenet") ne "standard")
	{
		my $beta = "groups.google.co.uk";
		$beta = "groups-beta.google.com" unless (loadrc("googlebeta") ne "yes");
		$url =~ s/\///g;
		my ($group) = $url =~ /[a-zA-Z]:(.*)/;
		my $bork = "";
		$bork = "hl=xx-bork&" unless (loadrc("bork") ne "yes");
		return 'http://' . $beta . '/groups?' . $bork . 'group=' . $group;
	}

	# strip trailing spaces
	$url =~ s/\s*$//;

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

	# Yes, I heard you the first time
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

	# GMail has obviously never met ELinks
	if ($url =~ 'gmail\.google\.com')
	{
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
	if (-f $bookmarkfile and loadrc('collapse') eq 'yes')
	{
		open BOOKMARKS, "+<$bookmarkfile";
		my $bookmark;
		while (<BOOKMARKS>)
		{
			s/<folder folded="no">/<folder folded="yes">/;
			$bookmark .= $_;
		}
		seek(BOOKMARKS, 0, 0);
		print BOOKMARKS $bookmark;
		truncate(BOOKMARKS, tell(BOOKMARKS));
		close BOOKMARKS;
	}

	# Words of wisdom from ELinks the Sage
	if (loadrc('fortune') eq 'fortune')
	{
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
	while (<COOKIES>)
	{
		$line[$#line + 1] = tell if /^%$/;
	}
	srand();
	while (not $fortune)
	{
		seek(COOKIES, $line[int rand($#line)], 0);
		while (<COOKIES>)
		{
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
	my $answer = '';
	open RC, "<$configperl" or return $answer;
	while (<RC>)
	{
		s/\s*#.*$//;
		next unless (m/(.*):\s*(.*)/);
		my $setting = $1;
		my $switch = $2;
		next unless ($setting eq $preference);
		if ($switch =~ /^(yes|1|on|yea|yep|sure|ok|okay|yeah|why.*not)$/)
		{
			$answer = "yes";
		}
		elsif ($switch =~ /^(no|0|off|nay|nope|nah|hell.*no)$/)
		{
			$answer = "no";
		}
		else
		{
			$answer = lc($switch);
		}
	}
	close RC;
	return $answer;
}


################################################################################
### Search engines #############################################################
my %search_engines = (
	"elgoog"     => {
		home     => 'http://alltooflat.com/geeky/elgoog/m/index.cgi',
		search   => 'http://alltooflat.com/geeky/elgoog/m/index.cgi?page=%2fsearch&cgi=get&q='
	},
	"google"     => {
		home     => 'http://google.com!bork!',
		search   => 'http://google.com/search?!bork!q='
	},
	"yahoo"      => {
		home     => 'http://yahoo.com',
		search   => 'http://search.yahoo.com/search?p='
	},
	"ask jeeves" => {
		home     => 'http://ask.com',
		search   => 'http://web.ask.com/web?q='
	},
	"a9"         => {
		home     => 'http://a9.com',
		search   => 'http://a9.com/?q='
	},
	"altavista"  => {
		home     => 'http://altavista.com',
		search   => 'http://altavista.com/web/results?q='
	},
	"msn"        => {
		home     => 'http://msn.com',
		search   => 'http://search.msn.com/results.aspx?q='
	},
	"dmoz"       => {
		home     => 'http://dmoz.org',
		search   => 'http://search.dmoz.org/cgi-bin/search?search='
	},
	"dogpile"    => {
		home     => 'http://dogpile.com',
		search   => 'http://dogpile.com/info.dogpl/search/web/'
	},
	"mamma"      => {
		home     => 'http://mamma.com',
		search   => 'http://mamma.com/Mamma?query='
	},
	"webcrawler" => {
		home     => 'http://webcrawler.com',
		search   => 'http://webcrawler.com/info.wbcrwl/search/web/'
	},
	"netscape"   => {
		home     => 'http://search.netscape.com',
		search   => 'http://channels.netscape.com/ns/search/default.jsp?query='
	},
	"lycos"      => {
		home     => 'http://lycos.com',
		search   => 'http://search.lycos.com/default.asp?query='
	},
	"hotbot"     => {
		home     => 'http://hotbot.com',
		search   => 'http://hotbot.com/default.asp?query='
	},
	"excite"     => {
		home     => 'http://search.excite.com',
		search   => 'http://search.excite.com/info.xcite/search/web/'
	},
);

sub search
{
	my ($engine, $search) = @_;
	my $key = $search ? 'search' : 'home';

	# Google is the default, Google is the best!
	$engine = 'google' unless $search_engines{$engine}
		and $search_engines{$engine}->{$key};
	my $url = $search_engines{$engine}->{$key};
	if ($engine eq 'google')
	{
		my $bork = '';
		if (loadrc('bork') eq 'yes')
		{
			if (not $search)
			{
				$bork = "/webhp?hl=xx-bork";
			}
			else
			{
				$bork = "hl=xx-bork&";
			}
		}
		$url =~ s/!bork!/$bork/;
	}
	$url .= $search if $search;
	return $url;
}


################################################################################
### News servers ###############################################################
my %news_servers = (
	"bbc"       => {
		home    => 'http://news.bbc.co.uk',
		search  => 'http://newssearch.bbc.co.uk/cgi-bin/search/results.pl?q=',
	},
	# The bastard child of Microsoft and the National Broadcasting Corporation
	"msnbc"     => {
		home    => 'http://msnbc.com',
		search  => 'http://msnbc.msn.com/?id=3053419&action=fulltext&querytext=',
	},
	"cnn"       => {
		home    => 'http://cnn.com',
		search  => 'http://search.cnn.com/pages/search.jsp?query=',
	},
	"fox"       => {
		home    => 'http://foxnews.com',
		search  => 'http://search.foxnews.com/info.foxnws/redirs_all.htm?pgtarg=wbsdogpile&qkw=',
	},
	"google"    => {
		home    => 'http://news.google.com',
		search  => 'http://news.google.com/news?q=',
	},
	"yahoo"     => {
		home    => 'http://news.yahoo.com',
		search  => 'http://news.search.yahoo.com/search/news/?p=',
	},
	"reuters"   => {
		home    => 'http://reuters.com',
		search  => 'http://reuters.com/newsSearchResultsHome.jhtml?query=',
	},
	"eff"       => {
		home    => 'http://eff.org',
		search  => 'http://google.com/search?sitesearch=http://eff.org&q=',
	},
	"wired"     => {
		home    => 'http://wired.com',
		search  => 'http://search.wired.com/wnews/default.asp?query=',
	},
	"slashdot"  => {
		home    => 'http://slashdot.org',
		search  => 'http://slashdot.org/search.pl?query=',
	},
	"newsforge" => {
		home    => 'http://newsforge.com',
		search  => 'http://newsforge.com/search.pl?query=',
	},
	"usnews"    => {
		home    => 'http://usnews.com',
		search  => 'http://www.usnews.com/search/Search?keywords=',
	},
	"newsci"    => {
		home    => 'http://newscientist.com',
		search  => 'http://www.newscientist.com/search.ns?doSearch=true&articleQuery.queryString=',
	},
	"discover"  => {
		home    => 'http://discover.com',
		search  => 'http://www.discover.com/search-results/?searchStr=',
	},
	"sciam"     => {
		home    => 'http://sciam.com',
		search  => 'http://sciam.com/search/index.cfm?QT=Q&SC=Q&Q=',
	},
);

sub news
{
	my ($server, $search) = @_;
	my $key = $search ? 'search' : 'home';

	# Fall back to the BBC if no preference.
	$server = 'bbc' unless $news_servers{$server}
		and $news_servers{$server}->{$key};
	my $url = $news_servers{$server}->{$key};
	$url .= $search if $search;
	return $url;
}


################################################################################
### Locators ###################################################################
my %locators = (
	'imdb'        => {
		home      => 'http://imdb.com',
		search    => 'http://imdb.com/Find?select=All&for=',
	},
	'stock'       => {
		home      => 'http://nasdr.com',
		search    => 'http://finance.yahoo.com/l?s=',
	},
	'bs'          => {
		home      => 'http://snopes.com',
		search    => 'http://search.atomz.com/search/?sp-a=00062d45-sp00000000&sp-q=',
	},
	'torrent'     => {
		home      => 'http://isohunt.com',
		search    => 'http://google.com/search?q=filetype:torrent !query!!bork!',
	},
	'archive'     => {
		home      => 'http://web.archive.org/web/*/!current!',
		search    => 'http://web.archive.org/web/*/',
	},
	'freshmeat'   => {
		home      => 'http://freshmeat.net',
		search    => 'http://freshmeat.net/search/?q=',
	},
	'sourceforge' => {
		home      => 'http://sourceforge.net',
		search    => 'http://sourceforge.net/search/?q=',
	},
	'savannah'    => {
		home      => 'http://savannah.nongnu.org',
		search    => 'http://savannah.nongnu.org/search/?type_of_search=soft&words=',
	},
	'gna'         => {
		home      => 'http://gna.org',
		search    => 'https://gna.org/search/?type_of_search=soft&words=',
	},
	'dead'        => {
		home      => 'http://www.whosaliveandwhosdead.com',
		search    => 'http://google.com/search?btnI&sitesearch=http://whosaliveandwhosdead.com&q=',
	},
	'book'        => {
		home      => 'http://gutenberg.org',
		search    => 'http://google.com/search?q=book+"!query!"',
	},
	'ipl'         => {
		home      => 'http://ipl.org',
		search    => 'http://ipl.org/div/searchresults/?words=',
	},
);

sub location
{
	my ($server, $search, $current_url) = @_;
	my $key = $search ? 'search' : 'home';

	croak 'Unknown URL!' unless $locators{$server}
		and $locators{$server}->{$key};
	my $url = $locators{$server}->{$key};

	my $bork = ""; $bork = "&hl=xx-bork" unless (loadrc("bork") ne "yes");
	$url =~ s/!bork!/$bork/g;

	$url =~ s/!current!/$current_url/g;
	$url .= $search if $search and not $url =~ s/!query!/$search/g;

	return $url;
}


=head1 SEE ALSO

elinks(1), perl(1)


=head1 AUTHORS

Russ Rowan, Petr Baudis

=cut
