# * Copyright (c) 2017 Stanislav Yudin <stan@endlessinsomnia.com>
# *
# * Permission to use, copy, modify, and distribute this software for any
# * purpose with or without fee is hereby granted, provided that the above
# * copyright notice and this permission notice appear in all copies.
# *
# * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

import os
import sys
import mmap

import html
import datetime
import urllib.parse

from kore import *

def on_load(state):
	print('python %s' % ('loading' if state == MODULE_LOAD else 'unloading'))
	log(LOG_DEBUG, 'python worker pid: %d' % os.getpid())

def method_name(mth):
	if mth == METHOD_GET:
		return 'GET'
	if mth == METHOD_POST:
		return 'POST'
	if mth == METHOD_PUT:
		return 'PUT'
	if mth == METHOD_DELETE:
		return 'DELETE'
	if mth == METHOD_HEAD:
		return 'HEAD'
	return 'UNKNOWN'

def minimal(req):
	req.response(200, b'')

def hello_world(req):
	print('Hello world from pyton! Got request=%s' % repr(req))
	print('\tagent: %s' % req.agent)
	print('\tmethod: %s' % method_name(req.method))
	print('\thost: %s' % req.host)
	print('\tpath: %s' % req.path)
	print('\tbody: %s' % req.body)

	req.response_header('Content-Type', 'text/plain')

	body = 'Hello World, time is %s' % datetime.datetime.now().isoformat()
	req.response(200, body.encode('utf-8'))
	print('Bye!')

#
# Auth & Validator Example
#

private_html = '''
<!DOCTYPE>
<html>
<head>
	<link rel="stylesheet" href="/css/style.css" type="text/css">
	<title>Kore Authentication tests</title>
</head>

<body>

<div class="content">
	<p style="font-size: small">The cookie session_id should now be set.</p>
	<p style="font-size: small">You can continue to <a href="/private">view page handler in auth block</a></p>
</div>

</body>
</html>
'''

def validate_session(req, data):
	rc = RESULT_OK if data.decode('utf-8') == 'test123' else RESULT_ERROR
	print("Validating session_id=%s => %d" % (data, rc))
	return rc

def handle_auth(req):
	req.response_header("content-type", "text/html");
	req.response_header("set-cookie", "session_id=test123");
	req.response(200, private_html.encode('utf-8'))

def handle_private(req):
	req.response_header("content-type", "text/html");
	req.response(200, "<body><h1>This is private content!</h1></body>".encode('utf-8'))


#
# File Upload Example
#


upload_html = '''
<!DOCTYPE>
<html>
<head>
	<title>Python upload test</title>
</head>

<body style="overflow: auto">

<div class="content">
	<form method="POST" enctype="multipart/form-data">
		<input type="file" name="file">
		<input type="submit" value="upload">
	</form>

	<p style="font-size: 12px; font-weight: normal">{upload}</p>
</div>

<div>
<pre>
{body}
</pre>
</div>

</body>
</html>
'''


def handle_file_upload(req):
	print('handle_file_upload')
	req.response_header('Content-Type', 'text/html')

	if req.method == METHOD_GET:
		print('->rendering upload form')
		req.response(200,
			upload_html.format(
				upload='',
				body='').encode('utf-8'))

	if req.method == METHOD_POST:
		if req.bodyfd != -1:
			print("file is on disk")

		req.populate_multipart_form()
		f = req.get_file("file")
		if f:
			print("->got file!")
			print("  filename: %s" % f.filename)
			print("  length: %s" % f.length)
			print("  position: %s" % f.position)
			print("  offset: %s" % f.offset)

			# read file as utf-8 encoded text
			content = f.readall().decode('utf-8')

			# format html safe utf-8 encoded response
			req.response(200, upload_html.format(
				upload='%s is %d bytes' % (f.filename, f.length),
				body=html.escape(content)).encode('utf-8'))
		else:
			# format error response
			req.response(400, "No file received".encode('utf-8'))


video_html = '''
<!DOCTYPE>
<html>
<head>
	<link href="https:////vjs.zencdn.net/4.7/video-js.css" rel="stylesheet">
	<script src="https:////vjs.zencdn.net/4.7/video.js"></script>
	<title>Video stream over PyKore</title>
</head>

<body>

<video class="video-js vjs-default-skin" width="640"
    height="240" controls preload="auto" data-setup='{"example_option":true}'>
	<source src="/videos/small.ogv" type="video/ogg">
Your browser does not support the video tag.
</video>

</body>
</html>

'''

class Video(object):
	__opened__ = {}

	def __init__(self, path):
		self.path = path
		self.size = os.path.getsize(self.path)
		self.fd = open(self.path, 'r+b')
		self.data = mmap.mmap(self.fd.fileno(), 0)

	def close(self):
		self.data.close()
		self.fd.close()

	@classmethod
	def clear(cls, v):
		del cls.__opened__[v.path]
		v.close()
		print("Closed video: %s" % v.path)

	@classmethod
	def open(cls, path):
		if not '/videos' in path:
			raise Exception("Invalid video path")
		vpath = path[1:]
		if not os.path.exists(vpath):
			print ("No such video file: %s" % vpath)
			return None

		v = Video(vpath)
		cls.__opened__[v.path] = v
		print("Opened video: %s" % v.path)
		return v

	@classmethod
	def get(cls, path):
		return cls.__opened__.get(path[1:])


def video_page(req):
	req.response_header('content-type', 'text/html')
	req.response(200, video_html.encode('utf-8'))

def video_open(path):
	v = Video.get(path)
	if v != None:
		return v
	return Video.open(path)

def video_finish(req):
	v = Video.get(req.path)
	if not v:
		raise Exception("No video file to close")
	Video.clear(v)

def parse_range_header(hdr_range):
	sbytes, srange = hdr_range.split('=', 2)
	if not '-' in srange:
		return 0, None

	args = srange.split('-')
	start = int(args[0])
	end = None
	if len(args) > 1 and len(args[1]) > 0:
		end = int(args[1])

	return start, end

def video_stream(req):
	v = video_open(req.path)
	if not v:
		print("Failed to open %s" % req.path)
		req.response(404, "No such file to stream.".encode("utf-8"))
		return
	
	ext = os.path.splitext(req.path)[1]
	hdr_range = req.get_header('Range')
	status = 200

	print("Got Range = '%s'" % hdr_range)
	if hdr_range:
		if not '=' in hdr_range:
			Video.clear(v)
			req.response(416, b"")
			return

		start, end = parse_range_header(hdr_range)
		if end == None:
			end = v.size

		if start > end or start > v.size or end > v.size:
			Video.clear(v)
			req.response(416, b"")
			return

		crng = "bytes %ld-%ld/%ld" % (start, end - 1, v.size)
		print("Serving 206 Content-Range = '%s'" % crng)
		req.response_header("content-range", crng);
		status = 206

	else:
		start = 0
		end = v.size

	req.response_header("content-type", "video/%s" % ext)
	req.response_header("accept-ranges", "bytes")
	req.response_stream(status, v.data[start:end], video_finish)


#
# WebSockets example
#

def on_wsconnect(conn):
	print("websocket - connected %s" % conn.addr)

def on_wsdisconnect(conn):
	print("websocket - disconnected %s" % conn.addr)

def on_wsmessage(conn, op, data):
	print("websocket - op=%d len=%s " % (op, len(data)))
	conn.websocket_broadcast(op, data, WEBSOCKET_BROADCAST_GLOBAL)

def handle_wspage(req):
	req.response_header("content-type", "text/html")
	with open('./assets/websockets_frontend.html', 'r') as f:
		wshtml = '\n'.join(f.readlines())
		req.response(200, wshtml.encode('utf-8'))

def handle_wsconnect(req):
	print("websocket - start connection")
	req.websocket_handshake(on_wsconnect, on_wsdisconnect, on_wsmessage)
	

