use strict;

sub goto_url_hook {
	my $url = shift;
	my $current_url = shift;
	return $url;
}

sub follow_url_hook {
	my $url = shift;
	return $url;
}

sub pre_format_html_hook {
	my $url = shift;
	my $html = shift;
	return $html;
}

sub proxy_for_hook {
	my $url = shift;
	return $url;
}

sub quit_hook {
}
