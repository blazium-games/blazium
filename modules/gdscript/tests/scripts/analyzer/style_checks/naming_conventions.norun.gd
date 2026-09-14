signal HealthChanged

enum player_state {
	idle,
	Running,
}

const maxHealth = 100

var currentHealth = 100

class inventory_slot:
	var slot_id = 0

func TakeDamage(Amount):
	pass

# A trailing comma is expected after the last element when the closing bracket is
# on its own line, and not expected when it shares the last element's line.

enum Ok {
	ONE,
	TWO,
}

enum Missing {
	ONE,
	TWO # <- MISSING_TRAILING_COMMA (enum)
}

enum Unnecessary { ONE, TWO, } # <- UNNECESSARY_TRAILING_COMMA (enum)

const OK_ARRAY = [1, 2]
const BAD_ARRAY = [1, 2,] # <- UNNECESSARY_TRAILING_COMMA (array)
const OK_DICT = {"a": 1}
const BAD_DICT = {"a": 1,} # <- UNNECESSARY_TRAILING_COMMA (dictionary)

func check_multiline():
	var ok_array = [
		1,
		2,
	]
	var missing_array = [
		1,
		2 # <- MISSING_TRAILING_COMMA (array)
	]
	var ok_dict = {
		"a": 1,
	}
	var missing_dict = {
		"a": 1 # <- MISSING_TRAILING_COMMA (dictionary)
	}
	print(ok_array, missing_array, ok_dict, missing_dict)

# Hexadecimal numbers use uppercase letters for their digits. The "0x" prefix and
# "_" separators are not affected, and numbers without letters have nothing to check.

const OK_COLOR = 0xFB8C0B
const BAD_COLOR = 0xfb8c0b # <- HEXADECIMAL_CASE
const OK_MASK = 0xFFFF_F8F8_0000
const BAD_MASK = 0xffff_f8f8_0000 # <- HEXADECIMAL_CASE
const OK_NO_LETTERS = 0x1234
const OK_DECIMAL = 1_234_567_890
const OK_BINARY = 0b1101_0010_1010

func check_locals():
	var mixed_case = 0xDEADbeef # <- HEXADECIMAL_CASE
	var not_a_number = "0xfb8c0b"
	print(mixed_case, not_a_number)
