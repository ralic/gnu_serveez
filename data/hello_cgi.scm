#! /usr/local/bin/sizzle -s 
!#
;; Import the necessary modules.
(use-modules (net cgi))

;; Initialize the CGI module, so that we can access query variables.
(cgi:setup)

;; Print out the `Content-type' header.
(cgi:print-header)

;; All data output via `display' will be sent to the browser.

(display "<html>
  <head>
    <title>Hello World!</title>
  </head>
  <body>
    <h2>Hello World!</h2>
    <p>
      This is the output of a Sizzle CGI script.  CGI scripts can do
      everything other scripts can do, and they can access query variables
      passed from the web server.
    </p>
    <h2>Query variables</h2>
    <p>
      If you want to see something in the list below, just add a string like 
      <tt>?foo=bar&froz=braz&test=lala&foo=dum</tt> to the URL your
      browser is displaying.
    </p>
    <ul>
")

(for-each 
 (lambda (n)
   (display "      <li>")
   (display n)
   (display ": ")
   (display (string-join (cgi:query-values n) ", "))
   (display "</li>\n"))
 (cgi:query-keys))

(display (string-append "    </ul>
    <h2>Information about your Sizzle installation</h2>
    <ul>
      <li>Sizzle version: " %sizzle-version% "</li>
      <li>Load path: " (string-join %load-path ":") "</li>
    </ul>
  </body>
</html>
"))
