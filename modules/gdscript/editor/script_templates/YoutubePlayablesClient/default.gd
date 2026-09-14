# meta-description: Base template showcasing some basic functionality
extends _BASE_


func _ready() -> void:
	if not is_youtube_environment():
		print("Not in YouTube playables environment.")
		return

	print("SDK version: " + get_sdk_version())
	print("Audio enabled: " + ("Yes" if is_audio_enabled() else "No"))

	var result: YoutubePlayablesResult = await get_language().finished
	if result.has_error():
		push_error(result.error)
		log_error()
	else:
		print("User language: " + result.data)

	result = await send_score(42).finished
	if result.has_error():
		push_error(result.error)
		log_error()

	result = await open_yt_content("sp27ShY6hKU").finished
	if result.has_error():
		push_error(result.error)
		log_error()

	result = await save_data(JSON.stringify({"answer": 42})).finished
	if result.has_error():
		push_error(result.error)
		log_error()

	result = await load_data().response
	if result.has_error():
		push_error(result.error)
		log_error()
	else:
		var data: Dictionary = JSON.parse_string(result.data)
		print("The answer is, %d." % data["answer"])
