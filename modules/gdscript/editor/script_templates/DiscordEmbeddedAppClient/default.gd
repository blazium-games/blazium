# meta-description: Base template showcasing some basic functionality
extends _BASE_


func _init() -> void:
	log_updated.connect(_log_updated)

	# Only run this on discord embedded app environment
	if is_discord_environment():
		var result:DiscordEmbeddedAppResult = await is_ready().finished
		if result.has_error():
			push_error(result.error)
			return


func do_authorize() -> void:
	var result = await authorize("code", "", "none", ["identify"]).finished
	if result.data.has("code"):
		print(result.data)
	else:
		push_error("Cannot authorize ", result.data)


func _log_updated(command: String, message: String):
	print("DiscordEmbeddedAppClient: ", command, ": ", message)
