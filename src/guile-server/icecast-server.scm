;; -*-scheme-*-
;;
;; icecast-server.scm - very simplified icecast server
;;
;; Copyright (C) 2002 Stefan Jahn <stefan@lkcc.org>
;;
;; This is free software; you can redistribute it and/or modify it
;; under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.
;; 
;; This software is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;; General Public License for more details.
;; 
;; You should have received a copy of the GNU General Public License
;; along with this package; see the file COPYING.  If not, write to
;; the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
;; Boston, MA 02111-1307, USA.
;;
;; $Id: icecast-server.scm,v 1.1 2002/05/24 12:51:13 ela Exp $
;;

;; load convenience file
(primitive-load "serveez.scm")

;; server initialisation
(define (icecast-init server)
  (let ((dir '()))

    ;; open directory
    (catch #t
	   (lambda ()
	     (set! dir (opendir (svz:server:config-ref server "directory"))))
	   (lambda args
	     (println "icecast: no such directory `"
		      (svz:server:config-ref server "directory") "'")
	     ))

    ;; collect files
    (if (directory-stream? dir)
	(let* ((files '()))
	  (let loop ((file (readdir dir)))
	    (if (not (eof-object? file))
		(begin
		  ;; filter MP3 files
		  (if (and (> (string-length file) 3)
			   (equal? (substring file (- (string-length file) 3))
				   "mp3"))
			   (set! files (cons file files)))
		  (loop (readdir dir)))))

	  ;; close directory
	  (closedir dir)

	  ;; state loaded files
	  (if (<= (length files) 0)
	      (begin
		(println "icecast: no files found in `" 
			 (svz:server:config-ref server "directory") "'")
		-1)
	      (begin
		(println "icecast: " (length files) " file(s) found in `" 
			 (svz:server:config-ref server "directory") "'")
		(svz:server:state-set! server "files" files)
		0)))
	-1)))

;; protocol detection
(define (icecast-detect-proto server sock)
  (let ((idx (binary-search (svz:sock:receive-buffer sock) "GET "))
	(addr (svz:sock:remote-address sock)))
    (if (and idx (= idx 0))
        (begin
	  (println "icecast: client detected at " (svz:inet-ntoa (car addr)))
          -1)
        0)))

;; connecting a client
(define (icecast-connect-socket server sock)
  (let* ((reply (string-append 
		 "HTTP/1.0 200 OK\r\n"
		 "Content-Type: audio/x-mp3stream\r\n"
		 "Cache-Control: no-cache\r\n"
		 "Pragma: no-cache\r\n"
		 "Connection: close\r\n"
		 "x-audiocast-name: "
		 (svz:server:config-ref server "server")
		 "\r\n\r\n"))
	 (data (make-hash-table 3)))

    ;; save state in socket
    (hash-set! data "seed" (seed->random-state (current-time)))
    (svz:sock:data sock data)

    ;; resize send buffer
    (svz:sock:send-buffer-size sock 
			       (svz:server:config-ref server "buffer-size"))

    ;; get first file
    (icecast-next-file sock)

    ;; setup the trigger functionality for mpeg streaming
    (svz:sock:trigger-condition sock icecast-trigger-condition)
    (svz:sock:trigger sock icecast-trigger)
    
    ;; send initial HTTP header
    (svz:sock:print sock reply)))

;; read part of a file
(define (icecast-readbuffer port size)
  (svz:read-file port size))

;; old *VERY SLOW* read
(define (icecast-readbuffer-snail port size)
  (let loop ((chars '()) (n 0))
    (let ((char (read-char port)))
      (if (or (>= n size) (eof-object? char))
          (list->string (reverse! chars))
          (loop (cons char chars) (1+ n))))))

;; close the currently streamed file
(define (icecast-close-file data)
  (let* ((port (hash-ref data "port")))
    (if (input-port? port) (close port))
    (hash-remove! data "port")
    (hash-remove! data "file")))

;; choose next mpeg file in stream
(define (icecast-next-file sock)
  (let* ((server (svz:sock:server sock))
	 (data (svz:sock:data sock))
	 (seed (hash-ref data "seed"))
	 (files (svz:server:state-ref server "files"))
	 (n (random (length files) seed))
	 (file (list-ref files n))
	 (port '()))

    ;; close current file
    (icecast-close-file data)

    ;; open next file
    (set! file (string-append (svz:server:config-ref server "directory") 
			      "/" file))
    (catch #t
	   (lambda ()
	     (set! port (open-file file "rb")))
	   (lambda args
	     (println "icecast: unable to open `" file "'")))

    (if (input-port? port)
	(begin
	  (hash-set! data "file" file)
	  (hash-set! data "port" port)
	  (println "icecast: uploading `" file "'")))))

;; ensure the send buffer is filled
(define (icecast-trigger-condition sock)
  (let*	((size (svz:sock:send-buffer-size sock))
	 (read-bytes (- (car size) (cdr size))))
    (> read-bytes 0)))

;; stream mpeg file data
(define (icecast-trigger sock)
  (let* ((data (svz:sock:data sock))
	 (size (svz:sock:send-buffer-size sock))
	 (read-bytes (- (car size) (cdr size))))

    (let* ((port (hash-ref data "port")) (buffer '()))
      (if (not (input-port? port))
	  (icecast-next-file sock)
	  (begin
	    (set! buffer (icecast-readbuffer port read-bytes))
	    (if (eof-object? buffer)
		(icecast-next-file sock)
		(svz:sock:print sock buffer)))))
  0))

;; server type definitions
(define-servertype! '(
  (prefix          . "icecast")
  (description     . "guile icecast server")
  (detect-proto    . icecast-detect-proto)
  (init            . icecast-init)
  (connect-socket  . icecast-connect-socket)
  (configuration   . (
    (directory       . (string  #t "/"))
    (server          . (string  #t "serveez: guile icecast server"))
    (buffer-size     . (integer #t 8192)) 
  ))))

;; server instantiation
(define-server! 'icecast-server `(
	 (directory   . "/usr/local/ftp/pub")
	 (server      . "guile-serveez-ice cast server")
	 (buffer-size . ,(* 64 1024))
	 ))

;; default port configuration
(define-port! 'icecast-port '((proto . tcp)
			      (port  . 8000)))

;; bind server to port
(bind-server! 'icecast-port 'icecast-server)

;;
;; please note: voice-over-ip setup
;;
;; $ mkfifo voip.mp3
;; $ sox -t ossdsp -w -s /dev/audio -t wav - | lame -a -f -b 32 - > voip.mp3
;;
