# meta-description: Base template showcasing some basic functionality
extends _BASE_

func _init() -> void:
	received_jwt.connect(_received_jwt)
	log_updated.connect(_log_updated)

	# Connect to the server
	var result = await connect_to_server().finished
	if result.has_error():
		push_error(result.error)

func request_login_and_open() -> void:
	# Get Login URL
	var login_result :LoginURLResult = await request_login_info("discord").finished
	if login_result.has_error():
		push_error(login_result.error)
		return

	# Open Login URL and wait for received_jwt signal
	var error := OS.shell_open(login_result.login_url)
	if error != OK:
		push_error(error)

func _log_updated(command: String, message: String):
	print("LoginClient: ", command, ": ", message)

func _received_jwt(jwt: String, type: String, access_token: String):
	print("Got jwt and access_token", jwt, " ", type, " ", access_token)
