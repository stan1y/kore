import kore
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

def python_minimal(req):
	req.response(200, b'')

def hello_world(req):
	print('python -> on_request: %s' % repr(req))
	print('\tagent: %s' % req.agent)
	print('\tmethod: %s' % method_name(req.method))
	print('\thost: %s' % req.host)
	print('\tpath: %s' % req.path)
	print('\tbody: %s' % req.body)

	req.response_header('Content-Type', 'text/plain')

	body = 'Hello World, time is %s' % datetime.datetime.now().isoformat()
	req.response(200, body.encode('utf-8'))


upload_html = '''
<!DOCTYPE>
<html>
<head>
	<link rel="stylesheet" href="/css/style.css" type="text/css">
	<title>Kore upload test</title>
</head>

<body style="overflow: auto">

<div class="content">
	<form method="POST" enctype="multipart/form-data">
		<input type="file" name="file">
		<input type="submit" value="upload">
	</form>

	<p style="font-size: 12px; font-weight: normal">{upload}</p>
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
			upload_html.format(upload='').encode('utf-8'))

	if req.method == kore.METHOD_POST:
		if req.bodyfd != -1:
			print("file is on disk")

		req.populate_multipart_form()
		file = req.get_file("file")
		if file:
			print("->got file!")
			print("  filename: %s" % file.filename)
			print("  length: %s" % file.length)
			print("  position: %s" % file.position)
			print("  offset: %s" % file.offset)

			answer = upload_html.format(upload='%s is %d bytes' % 
					(file.filename, file.length))

			req.response(200, answer.encode('utf-8'))
		else:
			req.response(400, "No file received".encode('utf-8'))