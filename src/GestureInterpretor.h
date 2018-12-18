#ifndef GESTURE_INTERPRETOR_H
#define GESTURE_INTERPRETOR_H

#include <thread>
#include <future>

#include <Godot.hpp>
#include <Control.hpp>
#include <Array.hpp>
#include <Dictionary.hpp>
#include <Vector2.hpp>

#include <String.hpp>
#include <Viewport.hpp>
#include <Input.hpp>
#include <Color.hpp>
using namespace godot;

namespace godot {
	class GestureInterpretor : public Control {
		GODOT_CLASS(GestureInterpretor, Control)

		private:
			Dictionary* gestures; // The gestures we are to detect
			Dictionary* biases; // The biases for each one of these gestures (this is added to the distance)
			Array* points; // The points currently recorded from mouse input
			float maximum_allowed_distance; // The maximum allowed distance (tolerance) for detection

		public:
			// Function to register methods of this module, construtor, and destructor
			static void _register_methods();
			GestureInterpretor();
			~GestureInterpretor();

			// Static utility functions
			static Array minmax_transform(Array points);
			static Array trim_points(Array points, int expected);
			static float compare_pointlists(Array reference, Array points);

			// Exported functions that godot actually uses
			void _init();
			void _process(float delta);
			void _draw();
			void set_gestures(Dictionary newGestures);
			void set_options(Dictionary biases, Variant tolerance);
			void set_biases(Dictionary biases);
			Variant check_gesture(Array points);
	};
}

#endif