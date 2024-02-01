# Lerchensporn/minify

This is a minifier for CSS, JavaScript, XML, HTML and JSON, written in C. It is
designed according to the following requirements:

- Released as single binary with no dependencies except libc.

- This tool is intended to be run by Makefile recipes to copy & minimize static assets from a
  `src/` folder to a `build/release/` folder. An additional debug make-target may omit the
  minification.  It is not envisioned to create bindings for scripting languages. Just use the
  command-line interface, or if really needed, usage via FFI should work fine.

- Produce standard-conform output from any standard-conform input.

- The program should fail on syntax errors and must not fix them.

- This minifier is not a cleaner. It should not modify the semantics of the markup.

- Specifially, non-objectives are cleanups that can (and should) be done in the
  source code if they don't decrease the legibility.  Detection of such
  optimization potential is in the scope of a linter. Linting may be added in
  the future but it would be a separate functionality.

   - Not removing empty CSS rules. Why would you have empty rules in the released code?
   - No merging of CSS properties.
   - Not removing doctypes or XML headers. They do have a meaning.
   - Minification of colors is a possible future objective because doing it in
     the source code can decrease its legibility.
   - Mangling of JavaScript identifiers is a possible future objective.
   - Not collapsing boolean HTML attributes or omitting `type=text/javascript`.

- Cleaning exported SVG files is better done with a cleaner such as `svgcleaner`.
  With default options, such cleaners substantially modify the markup
  semantically, which may be desired for exported SVG files but not for
  hand-written ones.  This minifier is suitable to minify hand-written SVG
  files.

  A useful manual cleaning recipe for exported SVG files may be:
  ```
  svgcleaner picture.svg clean.svg
  XMLLINT_INDENT="    " xmllint --format clean.svg > picture.svg
  rm clean.svg
  ```
  This generates standard-compliant XML files with indentation to enable manual
  inspection or adjustment. In a Makefile one can run this minifier on the
  result to create the release.

# Alternatives

Some alternative minifiers choose different objectives.

 - Many minifiers use advanced minification strategies that require large
   dependencies to parse the input into an abstract syntax tree. Usually they
   are focused on only one input format and provide many configuration options.
   If you want maximum minification, prefer such specialized tools.

   This minifier is rather focused on maximizing the web developer
   productivity: having only one lightweight tool with few options to handle
   all relevant formats. Moreover its approach enables the best possible
   runtime performance by looping through the input in a single pass.

 - Some minifiers semantically alter the content of the input files, such as
   fixing errors or removing doctypes, version tags, XML namespaces and
   encoding information.

   Besides the fact that some of the information can be
   relevant, my opinion is that these are different concerns that should be
   handled by separate tools, namely linters or cleaners, and that a minifier
   should assume (or assert) perfect quality of the input files.

   To illustrate why these are separate concerns, consider that minifiers (and
   linters) are supposed to run automatically in every build process, whereas
   cleaners or fixers are rather run manually.

 - Some minifiers are quite fault-tolerant. However, if garbage is fed into a
   minifier then there is a problem somewhere else in the tool or process chain
   that is good to be alerted about. Strict parsing is a good practice to
   promote standard conformity.

## Alternative multi-format minifier

https://github.com/tdewolff/minify

Written in Go. Minifies HTML, XML, SVG, JS, CSS, JSON. It is in the package
repository of Alpine Linux and statically linked binary releases exist.

## Alternative CSS minifiers

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

## Alternative JavaScript minifiers

https://terser.org/

https://github.com/mishoo/UglifyJS

https://swc.rs/playground

https://bun.sh/

https://esbuild.github.io/

## SVG optimizers

https://github.com/RazrFalcon/svgcleaner

Written in Rust.

https://github.com/svg/svgo

Written in JavaScript / NodeJS.

https://github.com/scour-project/scour

Written in Python.

## Alternative HTML and XML minifiers

Here we have two very good options with no big dependencies. I still included
XML and HTML minification in this minifier to cover all needs with one tool.

https://www.html-tidy.org/

Written in C.

`xmllint` as part of libxml2 (https://gitlab.gnome.org/GNOME/libxml2)

Written in C.

