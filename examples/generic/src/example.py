import kore
import datetime
import urllib.parse

def on_load(state):
	print('python -> on_load: %s' % ('loading' if state == kore.MODULE_LOAD else 'unloading'))
	return kore.RESULT_OK

def on_request(req):
	print('python -> on_request: %s' % repr(req))
	print('\tagent: %s' % req.agent)
	print('\thost: %s' % req.host)
	print('\tpath: %s' % req.path)
	print('\tbody: %s' % req.body)

	kore.http_response_header(req, 'Content-Type', 'text/plain')

	body = 'Hello World, time is %s' % datetime.datetime.now().isoformat()
	kore.http_response(req, 200, body.encode('utf-8'))
    