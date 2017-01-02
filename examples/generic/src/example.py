import kore
import urllib.parse

def on_load(state):
    pass

def on_request(req):
	print("on_request: %s" % repr(req))
	print("agent: %s" % req.agent)
	print("host: %s" % req.host)
	print("path: %s" % req.path)
	print("body: %s" % req.body)

	kore.http_response_header(req, 'Content-Type', 'text/plain')

	body = "Hello World, %s" % urllib.parse.quote(req.agent)
	kore.http_response(req, 200, body.encode('utf-8'))
    