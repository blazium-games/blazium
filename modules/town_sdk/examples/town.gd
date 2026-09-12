extends Node

@onready var town_client: Object = Engine.get_singleton("TownSDK")

# Example configuration values. Replace with your TownServer endpoint and JWT.
var server_address: String = "127.0.0.1"
var server_port: int = 7000
var jwt_token: String = ""

const ACTION_ATTACK: int = 0

func _ready() -> void:
	if town_client == null:
		push_error("TownSdkClient singleton not available. Ensure the Town SDK module is compiled into the engine.")
		return

	# Hook the most common signals (if the singleton exposes them).
	_connect_signal("connected", "_on_connected")
	_connect_signal("connection_failed", "_on_connection_failed")
	_connect_signal("error", "_on_error")
	_connect_signal("disconnected", "_on_disconnected")
	_connect_signal("snapshot_received", "_on_snapshot_received")
	_connect_signal("move_state", "_on_move_state")
	_connect_signal("battle_start", "_on_battle_start")
	_connect_signal("battle_state", "_on_battle_state")
	_connect_signal("battle_end", "_on_battle_end")
	_connect_signal("admin_stats", "_on_admin_stats")

	_connect_signal("reconnecting", func(attempt: int, next_delay: float) -> void:
		print("Reconnecting attempt #%s, next retry in %ss" % [attempt, next_delay]))
	_connect_signal("reconnect_failed", func(reason: String) -> void:
		push_warning("Reconnection failed: %s" % reason))

	# Kick off the initial connection.
	var ok_bool: bool = _call_client("connect_to_server", [server_address, server_port]) as bool
	if not ok_bool:
		push_error("Initial connection attempt could not be started.")


func _process(delta: float) -> void:
	if town_client:
		_call_client_void("poll", [delta])


func _on_connected() -> void:
	print("Connected to TownServer at %s:%s" % [server_address, server_port])
	if jwt_token != "":
		_call_client_void("authenticate", [jwt_token])
	else:
		print("No JWT token provided; server may restrict available actions.")

	# Example region entry and admin stats request.
	_call_client_void("enter_region", ["tutorial"])
	_call_client_void("admin_stats_request")


func _on_connection_failed() -> void:
	push_error("Failed to reach TownServer at %s:%s" % [server_address, server_port])


func _on_disconnected(reason: String) -> void:
	push_warning("Disconnected from TownServer: %s" % reason)


func _on_error(message: String) -> void:
	push_error("Town SDK Error: %s" % message)


func _on_snapshot_received(snapshot: Dictionary) -> void:
	print("Received region snapshot:", snapshot)


func _on_move_state(state: Dictionary) -> void:
	# Update local entities here.
	pass


func _on_battle_start(battle: Dictionary) -> void:
	print("Battle started:", battle)
	# Example: choose to attack automatically.
	var battle_id: String = battle.get("id", "") as String
	if battle_id != "":
		_call_client_void("battle_action", [battle_id, ACTION_ATTACK])


func _on_battle_state(_state: Dictionary) -> void:
	# Track turn order or UI updates.
	pass


func _on_battle_end(battle: Dictionary) -> void:
	print("Battle ended:", battle)


func _on_admin_stats(payload: Dictionary) -> void:
	print("Admin stats:", payload)


func _connect_signal(signal_name: String, callback: Variant) -> void:
	if town_client != null and town_client.has_signal(signal_name):
		if callback is Callable:
			town_client.connect(signal_name, callback)
		elif callback is String:
			town_client.connect(signal_name, Callable(self, callback))


func _call_client(method: String, args: Array = []) -> Variant:
	if town_client != null and town_client.has_method(method):
		return town_client.callv(method, args)
	push_warning("TownSdkClient missing method %s" % method)
	return null


func _call_client_void(method: String, args: Array = []) -> void:
	_call_client(method, args)
