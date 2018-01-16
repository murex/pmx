if [ -n "$LC_CTYPE" ]; then
	lc_ctype=$LC_CTYPE;
	unset LC_CTYPE;
fi

libtoolize && aclocal \
&& automake --add-missing \
&& autoconf

#export LC_CTYPE=$lc_ctype
