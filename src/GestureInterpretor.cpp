#include "GestureInterpretor.h"
#include <iostream>
//#define DEBUG_MODE
#define DRAW_TRAIL

using namespace godot;

// Register our exported methods
void GestureInterpretor::_register_methods() {
	register_method("_init", &GestureInterpretor::_init);
	register_method("_process", &GestureInterpretor::_process);
#ifdef DRAW_TRAIL
	register_method("_draw", &GestureInterpretor::_draw);
#endif
	register_method("set_gestures", &GestureInterpretor::set_gestures);
	register_method("set_options", &GestureInterpretor::set_options);
	register_method("set_biases", &GestureInterpretor::set_biases);
	register_method("check_gesture", &GestureInterpretor::check_gesture);
	register_signal<GestureInterpretor>((char *)"gesture_detected", "name", GODOT_VARIANT_TYPE_STRING);
}

// Constructor and destructor.
GestureInterpretor::GestureInterpretor() {
	gestures = NULL; // Note that godot may not have been initialized yet, and hence we can't create a new dictionary here
	biases = NULL;
	points = NULL;
	maximum_allowed_distance = 0.028;
}
GestureInterpretor::~GestureInterpretor() {
	if (gestures) delete gestures; // Deconstructor deletes gestures and biases if they exist
	if (biases) delete biases;
	if (points) delete points;
}

// This is our de-facto constructor.
void GestureInterpretor::_init() {
	gestures = new Dictionary(); // Initialize gestures and biases to empty dictionaries
	biases = new Dictionary();
	points = new Array();
}

// This is executed every frame
void GestureInterpretor::_process(float delta) {
	// Note: this can be tuned, but with this distance, the recommended minimum number of points per gesture entry
	// is 8. Too many points in a gesture entry is bad too, as it then only works when the user does a large gesture with
	// many points.
	const int minDistanceBetweenPoints = 16;

	// If mouse button is being pressed, record points
	if (Input::get_singleton()->is_mouse_button_pressed(1)) {
		Vector2 mousePos = get_viewport()->get_mouse_position();
		if (points->size() == 0 || mousePos.distance_to((*points)[(*points).size()-1]) > minDistanceBetweenPoints) {
			points->append(mousePos);
		}
	} else if (points->size() != 0) {
		// Once mouse button is released, check the gesture and clear points
		String result = check_gesture(*points);
		if (result.length() != 0) { // If the string is of at least length 1 (not empty), a gesture was detected
			emit_signal("gesture_detected", result);
		}
		points->clear();
	}

#ifdef DRAW_TRAIL
	update(); // Draw UI stuff
#endif
}

// This draws stuff to the screen
void GestureInterpretor::_draw() {
	for (int i=0; i<(*points).size(); i++) {
		draw_circle((*points)[i], 2, Color(1,1,1,0.5));
	}
}

// Exported function that sets the gestures dictionary.
void GestureInterpretor::set_gestures(Dictionary newGestures) {
	// Delete our old gestures and initialize a new one
	delete gestures;
	gestures = new Dictionary();
	// Set keys and values in gestures to the keys and values in newGestures, minmax transformed.
	Array keys = newGestures.keys();
	Array values = newGestures.values();
	for (int i=0; i<keys.size(); i++) {
		(*gestures)[keys[i]] = minmax_transform(values[i]);
	}
}

// Exported function that sets the biases dictionary only.
void GestureInterpretor::set_biases(Dictionary newBiases) {
	delete biases;
	biases = new Dictionary(newBiases);
}
// Exported function that sets the biases dictionary and the maximum_allowed_distance variable.
void GestureInterpretor::set_options(Dictionary newBiases, Variant newTolerance) {
	set_biases(newBiases);
	maximum_allowed_distance = static_cast<float>(newTolerance);
}

// Exported function that takes in an array of points and returns either an empty string if no
// gesture was detected or the name of the gesture detected.
Variant GestureInterpretor::check_gesture(Array points_raw) {
	// Transform points using minmax transform
	Array points = minmax_transform(points_raw);

#ifndef DEBUG_MODE
	// Get the distance of each gesture from the point array on multiple threads.
	std::future<float> futures[gestures->size()];
	Array gestureValues = gestures->values();
	for (int i=0; i<gestures->size(); i++) {
		futures[i] = std::async(compare_pointlists, gestureValues[i], points);
	}
#endif

	// Loop through each gesture, finding the one with the lowest distance below the maximum allowed distance.
	float minimum_distance = maximum_allowed_distance, current_distance;
	int minimum_index = -1;
	Array gestureKeys = gestures->keys();
	for (int i=0; i<gestures->size(); i++) {
		// Start with the bias, then add the distance and compare
		// Note: if this returns NULL, that gets casted to 0.0, which is what we want if no bias is set
		current_distance = static_cast<float>((*biases)[gestureKeys[i]]);
#ifdef DEBUG_MODE
		current_distance += compare_pointlists(gestures->values()[i], points);
		std::cout << "[GDNative Debug] Distance: " << current_distance << '\n';
#else
		current_distance += futures[i].get();
#endif
		if (current_distance < minimum_distance) {
			minimum_distance = current_distance;
			minimum_index = i;
		}
	}

	// If no gesture was selected, return an empty string.
	// Otherwise, return the string corresponding with the selected gesture.
	if (minimum_index == -1) {
		return String("");
	} else {
		return gestures->keys()[minimum_index];
	}
}

// Static utility function that finds the difference between two arrays of points
float GestureInterpretor::compare_pointlists(Array reference, Array points) {
	// Constants
	const float duplicate_aversion = 0.02; // Penalty added to duplicate count squared
	const float skip_aversion = 0.04; // Penalty added to skipped points

	// First, trim points so that reference and points have the same number of points.
	points = trim_points(points, reference.size());

	// Loop through points, pairing them with the point in the reference of the least penalty
	int total_matches[reference.size()] = {}; // Total matches for each point in the reference array
	float currDistanceSquared; // Current distance squared between point in points and point in reference
	float currPenalty, minPenalty; // Current and minimum penalty for points in reference
	float totalPenalty = 0; // Total penalty for all points in points
	int minIndex; // Index of point in reference with minimum distance
#ifdef DEBUG_MODE
	std::cout << "POINTLIST" << ":";
#endif
	for (int i=0; i<points.size(); i++) {
		// Find the index with the least penalty
		minPenalty = INFINITY;
		minIndex = -1;
		for (int j=0; j<reference.size(); j++) {
			// Calculate penalty = duplicates * duplicate_aversion^2 - remove_skip * skip_aversion + distance^2
			currDistanceSquared = static_cast<Vector2>(reference[j]).distance_squared_to(points[i]);
			currPenalty = total_matches[j] * duplicate_aversion*duplicate_aversion -
				(total_matches[j] == 0) * skip_aversion +
				currDistanceSquared;
			// Check if penalty is less than minimum penalty, and if so, make this the new minimum
			if (currPenalty < minPenalty) {
				minPenalty = currPenalty;
				minIndex = j;
			}
#ifdef DEBUG_MODE
			std::cout << currPenalty << ' ';
#endif
		}
		// Increment the total matches for the index chosen and the total penalty for the array
#ifdef DEBUG_MODE
		std::cout << "->" << minPenalty << '\n';
#endif
		total_matches[minIndex]++;
		totalPenalty += minPenalty;
	}
	// Increment the total penalty by the number of points in reference multiplied by skip_aversion
	// (we subtracted unskipped points before; now we're adding skipped points)
	totalPenalty += reference.size() * skip_aversion;

	// Return the average penalty of the array.
	// This is the difference between the reference and points arrays.
	return totalPenalty/points.size();
}

// Static utility function that performs a minmax transform on an array of points
Array GestureInterpretor::minmax_transform(Array points) {
	// Loop through vectors, finding smallest and largest x and y values
	Vector2 currVector;
	float min_x=INFINITY, min_y=INFINITY, max_x=-INFINITY, max_y=-INFINITY;
	for (int i=0; i<points.size(); i++) {
		currVector = static_cast<Vector2>(points[i]);
		if (currVector.x < min_x) min_x = currVector.x;
		if (currVector.y < min_y) min_y = currVector.y;
		if (currVector.x > max_x) max_x = currVector.x;
		if (currVector.y > max_y) max_y = currVector.y;
	}
	// Do a minmax transform on the points
	float range_x=max_x-min_x, range_y=max_y-min_y;
	for (int i=0; i<points.size(); i++) {
		currVector = static_cast<Vector2>(points[i]);
		points[i] = Vector2((currVector.x-min_x)/range_x, (currVector.y-min_y)/range_y);
	}
#ifdef DEBUG_MODE
	std::cout << "[GDNative Debug] Transformed Points:";
	for (int i=0; i<points.size(); i++) {
		std::cout << '(' << ((Vector2)points[i]).x << ',' << ((Vector2)points[i]).y << "), ";
	}
	std::cout << '\n';
#endif
	// Return the transformed points
	return points;
}

// Static utility function that trims an array of points to an expected number of points
Array GestureInterpretor::trim_points(Array points, int expected) {
	// Keep every x points, where x = expected / points.size() as a decimal
	float putRatio = static_cast<float>(expected) / points.size();
	float putCounter = 0.0;
	Array outputPoints;
	for (int i=0; i<points.size(); i++) {
		putCounter += putRatio;
		if (putCounter >= 1) {
			outputPoints.append(points[i]);
			putCounter -= 1;
		}
	}
#ifdef DEBUG_MODE
	std::cout << "[GDNative Debug] Trimmed Points:";
	for (int i=0; i<outputPoints.size(); i++) {
		std::cout << '(' << ((Vector2)outputPoints[i]).x << ',' << ((Vector2)outputPoints[i]).y << "), ";
	}
	std::cout << '\n';
#endif
	return outputPoints;
}