# meta-description: Base template showcasing some basic functionality
extends _BASE_

func execute_and_log(query: SQLiteQuery, params: Array = []):
	print("Executing query.. ", query.query)
	var result := query.execute(params)
	if result.error:
		print("Query ", result.query, " has error: ", result.error, " error_code: ", result.error_code)

# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	# In memory database
	database = SQLiteDatabase.new()
	# Create column schemas
	var id_schema := SQLiteColumnSchema.create("id", TYPE_INT)
	id_schema.auto_increment = true
	id_schema.primary_key = true
	var name_schema := SQLiteColumnSchema.create("name", TYPE_STRING)
	# Create table
	execute_and_log(database.create_table("test", [id_schema, name_schema]))
	# Insert rows
	var values := [{"id": 1, "name": "a"}, {"id": 2, "name": "b"}]
	execute_and_log(database.insert_rows("test", values))
	# Insert row
	execute_and_log(database.insert_row("test", {"name": "me"}), ["me"])
	print(database.tables)
	print(database.get_columns("test"))
	ResourceSaver.save(database, "res://test.sqlite")
