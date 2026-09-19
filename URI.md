#   URI parsing

This Ragel parser implements the RFC 3986 grammar, with one deliberate
break: a scheme takes no dots.

##  The dotless scheme (DOG-051)

RFC 3986 §3.1 lets a scheme carry `.` as well as `+` and `-`.  We drop
the dot, because our own text is links first and URIs second: a bare
`x.c:12` must name line 12 of `x.c`, not the scheme `x.c` holding the
path `12`.  No scheme we use is dotted, and the registered dotted ones
(`soap.beep`, `iris.beep`, `z39.50r`) are dead weight.

Dropping the dot alone would leave `x.c:12` malformed, since a relative
reference's first segment takes no colon (RFC 3986 §4.2,
`segment_nz_nc`).  So `URI.lex:28` adds a second first-segment shape,
`segment_nz_dc`: a segment that holds a `.` before its first `:`, and a
`:` after it.  Such a head can never be a scheme — a scheme has no dot
before its colon — so the two alternatives of `PathNoscheme`
(`URI.lex:31`) are disjoint machines and ragel needs no scanner ordering
to tell a path from a scheme.

`x.c:12`, `HUNK.c:HUNKu8sMakeURI` and `a.b.c:'x'` therefore parse as a
PATH holding the whole text.  So does `a.b://host/p`: with no scheme
there is no authority either, and the `//host/p` tail is read as plain
path segments.  `foo:bar`, `mailto:x@y.z` and `http://h:80/p` are
untouched — see `test/URI.c` `URITestDotlessScheme` for the pinned set.
