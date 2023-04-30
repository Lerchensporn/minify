This is a CSS minifier written in C. It is designed according to the following
requirements:

- Released as single binary with no dependencies except libc. Many other CSS
  compressors unfortunately need a scripting language runtime and thus tons of
  dependencies.

- Produce standard-conform output from any standard-conform input and do not crash
  on any input. The program may but is not required to abort on invalid input.

- Non-objectives are cleanups that can (and should) be done in the CSS source
  code if they don't decrease the legibility. Detection of such optimization
  potential is in the scope of a linter. Linting may be added in the future but
  it would be a separate functionality.

   - Not removing empty rules. Why would you have empty rules in the released code?
   - No merging of properties.
   - Not removing unneeded quotes.
   - Minification of colors is a possible future objective because doing it in
     the source code can decrease its legibility.

## Alternatives

https://github.com/csstidy-c/csstidy

Written in C++, not maintained. Prints lots of warnings on valid input.

https://csstidy.sourceforge.net/usage.php

https://github.com/cssnano/cssnano

Written in JavaScript.

https://github.com/clean-css/clean-css

Written in JavaScript.

https://github.com/parcel-bundler/lightningcss

Written in Rust.

https://github.com/fmarcia/uglifycss

Written in JavaScript.

https://yui.github.io/yuicompressor/

Written in Java.

https://github.com/matthiasmullie/minify

Written in PHP. Based on regex, hence it breaks on standard-conform input such as:
`@import url(http://example.org/*)/**/;`

http://opensource.perlig.de/rcssmin/

Written in Python. Based on regex.
