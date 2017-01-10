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

import kore

import html
import datetime
import urllib.parse

def on_load(state):
	print('python -> on_load: %s' % ('loading' if state == kore.MODULE_LOAD else 'unloading'))

def method_name(mth):
	if mth == kore.METHOD_GET:
		return 'GET'
	if mth == kore.METHOD_POST:
		return 'POST'
	if mth == kore.METHOD_PUT:
		return 'PUT'
	if mth == kore.METHOD_DELETE:
		return 'DELETE'
	if mth == kore.METHOD_HEAD:
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

	if req.method == kore.METHOD_GET:
		print('->rendering upload form')
		req.response(200,
			upload_html.format(
				upload='',
				body='').encode('utf-8'))

	if req.method == kore.METHOD_POST:
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
	<source src="/video.ogg" type="video/ogg">
Your browser does not support the video tag.
</video>

</body>
</html>

'''

__videos__ = {}

def video_page(req):
	req.response(200, video_html.encode('utf-8'))

def video_open(path):
	v = __videos__.get(path)
	if v != None:
		return v

	fpath = os.path.join('videos', path) 
	if os.path.exists(fpath):
		v = open(fpath, 'rb')
		__videos__[path] = v
		printf("opened video file '%s'" % req.path)
	else:
		return None

	return v

def video_finish(req):
	v = __videos__.get(req.path)
	if not v:
		raise Exception("No video fd to close")
	v.close()
	del __videos__[req.path]
	print("closed video file '%s'" % req.path)

def parse_range_header(hdr_range):
	sbytes, srange = hdr_range.split('=', 2)
	if not '-' in srange:
		return 0, None

	args = srange.split('-')
	start = long(args[0])
	end = None
	if len(args) > 1:
		end = long(args[1])

	return start, end

def video_stream(req):
	v = video_open(req.path)
	if not v:
		req.response(404, "No such file to stream.")
		return
	
	sz = os.path.getsize(req.path)
	ext = os.path.splitext(req.path)[1]
	hdr_range = req.get_header('Range')
	status = 200

	if hdr_range:
		if not '=' in hdr_range:
			v.close()
			del __videos__[req.path]

			req.response(416, b"")
			return
		start, end = parse_range_header(hdr_range)

		if start > end or start > sz or end > sz:
			v.close()
			del __videos__[req.path]

			req.response(416, b"")
			return

		crng = "bytes %ld-%ld/%ld" % (start, end - 1, sz)
		print("serving 206 content_range='%s'" % crng)
		status = 206

	else:
		start = 0
		end = sz

	req.response_header("content-type", "video/%s" % ext)
	req.response_header("accept-ranges", "bytes")
	
	req.response_stream(status, b'', video_finish)



	

