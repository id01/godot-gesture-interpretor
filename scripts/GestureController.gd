extends Control

# Get the gesture detector and the label we are using to provide output
onready var gesture_detector = get_node('GestureInterpretor');
onready var output_label = get_parent().get_node('OutputLabel')

# Vector2 alias
func v(x, y):
	return Vector2(x,y)

# On node ready set up gesture detection
func _ready():
	# The gestures we are detecting
	var gestures = {
		'C': [v(0.33,0.944), v(0,0), v(-0.33,0.944), v(-0.66,0.66), v(-0.944,0.33), v(-1,0),
			v(-0.944,-0.33), v(-0.66,-0.66), v(-0.33,-0.944), v(0,-1), v(0.33,-0.944)], # C shape
		'O': [v(0,100), v(-66,66), v(-100,0), v(-66,-66), v(0,-100), v(66,-66), v(100,0), v(66,66)], # O shape
		'V': [v(0,0), v(25,50), v(50,100), v(75,150), v(100,200), v(125,150), v(150,100), v(175,50), v(200,0)] # V shape
	}
	
	# Add gestures and biases to Gesture detector
	gesture_detector.set_gestures(gestures)
	gesture_detector.set_biases({
		'O': 0.001, # O is usually chosen more, give it a disadvantage
		'V': -0.01 # V is usually chosen less, give it an advantage
	})

	# Connect gesture detection to function
	gesture_detector.connect("gesture_detected", self, "on_gesture")
	
# When we detect a gesture, show it from OutputLabel
func on_gesture(name):
	output_label.text += name + ' '