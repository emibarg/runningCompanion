import os
from kivy.app import App
from kivy.uix.screenmanager import ScreenManager, Screen
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.button import Button
from kivy.uix.label import Label
from kivy.graphics import Color, Line, Rectangle
from kivy.core.window import Window
from kivy.clock import Clock

# mapview imports
from kivy_garden.mapview import MapView, MapMarkerPopup, MapLayer


def parse_log_data(file_path):
    """Parse log file into sessions with coordinates."""
    sessions = []
    current = None
    
    try:
        with open(file_path, "r", encoding="utf-8") as fh:
            for raw in fh:
                line = raw.strip()
                if not line:
                    continue

                if line.startswith("==== New Session ===="):
                    if current:
                        sessions.append(current)
                    current = {"date": None, "time": None, "coords": [], "summary": None}
                    
                elif line.startswith("Date:"):
                    if current is None:
                        current = {"date": None, "time": None, "coords": [], "summary": None}
                    current["date"] = line.replace("Date:", "").strip()
                    
                elif line.startswith("Time:"):
                    if current is None:
                        current = {"date": None, "time": None, "coords": [], "summary": None}
                    current["time"] = line.replace("Time:", "").strip()
                    
                elif line.startswith("Session:"):
                    if current is None:
                        current = {"date": None, "time": None, "coords": [], "summary": None}
                    current["summary"] = line
                    
                else:
                    # Parse coordinate lines: 22:39:09,-31.45759,-64.16764, 2.0
                    parts = [p.strip() for p in line.split(",")]
                    if len(parts) == 4:
                        try:
                            timestamp = parts[0]
                            lat = float(parts[1])
                            lon = float(parts[2])
                            speed = float(parts[3])
                            if current is None:
                                current = {"date": None, "time": None, "coords": [], "summary": None}
                            current["coords"].append({
                                "lat": lat,
                                "lon": lon,
                                "speed": speed,
                                "timestamp": timestamp
                            })
                        except ValueError:
                            pass

            if current:
                sessions.append(current)

    except Exception as e:
        print(f"Error parsing file: {e}")

    print(f"Parsed {len(sessions)} sessions")
    return sessions


class RouteLayer(MapLayer):
    """Draws polyline on map."""
    
    def __init__(self, coords, **kwargs):
        super().__init__(**kwargs)
        self.coords = coords

    def reposition(self):
        mapview = self.parent
        if not self.coords or mapview is None:
            return

        points = []
        for c in self.coords:
            try:
                x, y = mapview.get_window_xy_from(
                    lat=c["lat"], 
                    lon=c["lon"], 
                    zoom=mapview.zoom
                )
                points.extend([x, y])
            except:
                pass

        self.canvas.clear()
        if len(points) >= 4:
            with self.canvas:
                Color(1, 0, 0, 0.8)
                Line(points=points, width=2)


class DragDropScreen(Screen):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.label = Label(
            text="\n\nDrag and drop your log file (.txt) onto this window\n",
            halign="center",
            valign="middle",
        )
        self.add_widget(self.label)
        Window.bind(on_drop_file=self._on_file_drop)

    def _on_file_drop(self, window, file_path_bytes, x, y):
        try:
            path = file_path_bytes.decode("utf-8")
        except:
            path = os.fsdecode(file_path_bytes)

        if not path.lower().endswith(".txt"):
            self.label.text = "Please drop a .txt file"
            return

        sessions = parse_log_data(path)
        if sessions:
            self.manager.sessions = sessions
            self.manager.current = "session_list"
        else:
            self.label.text = "Could not parse any sessions from that file"


class SessionListScreen(Screen):
    def on_pre_enter(self):
        self.clear_widgets()
        layout = BoxLayout(orientation="vertical")
        layout.add_widget(Label(
            text="Sessions found (click to view)", 
            size_hint_y=None, 
            height=50
        ))

        if not getattr(self.manager, "sessions", None):
            layout.add_widget(Label(text="No sessions loaded"))
        else:
            for i, s in enumerate(self.manager.sessions):
                summary = s.get("summary") or ""
                date = s.get("date") or "?"
                time = s.get("time") or "?"
                btn_text = f"Session {i+1}: {date} {time} — {summary}"
                btn = Button(text=btn_text, size_hint_y=None, height=60)
                btn.bind(on_press=lambda inst, idx=i: self.open_session(idx))
                layout.add_widget(btn)

        back_btn = Button(text="Back (drop another file)", size_hint_y=None, height=50)
        back_btn.bind(on_press=lambda b: setattr(self.manager, "current", "dragdrop"))
        layout.add_widget(back_btn)
        self.add_widget(layout)

    def open_session(self, idx):
        self.manager.selected_session = self.manager.sessions[idx]
        self.manager.current = "map"


class MapScreen(Screen):
    def on_pre_enter(self):
        self.clear_widgets()
        session = getattr(self.manager, "selected_session", None)
        
        if not session:
            self.add_widget(Label(text="No session selected"))
            return

        coords = session.get("coords", [])
        if not coords:
            self.add_widget(Label(text="No coordinates in this session"))
            return

        # Main layout
        main_layout = BoxLayout(orientation="vertical")

        # Header
        header = BoxLayout(size_hint_y=None, height=50)
        header.add_widget(Label(
            text=f"Session: {session.get('summary','')}",
            size_hint_x=0.9
        ))
        back_btn = Button(text="Back", size_hint_x=0.1)
        back_btn.bind(on_press=lambda b: setattr(self.manager, "current", "session_list"))
        header.add_widget(back_btn)

        # Create map
        map_view = MapView(zoom=15)
        Clock.schedule_once(
            lambda dt: map_view.center_on(coords[0]["lat"], coords[0]["lon"]), 
            0.1
        )

        # Add route
        route_layer = RouteLayer(coords)
        map_view.add_layer(route_layer)

        # Add markers
        for coord in coords:
            self.create_marker(map_view, coord)

        main_layout.add_widget(header)
        main_layout.add_widget(map_view)
        self.add_widget(main_layout)

    def create_marker(self, map_view, coord):
        """Create a single marker with speed and time info."""
        marker = MapMarkerPopup(lat=coord["lat"], lon=coord["lon"])
        
        # Container
        container = BoxLayout(
            orientation="vertical",
            size_hint=(None, None),
            size=(150, 80)
        )
        
        # Background
        with container.canvas.before:
            Color(0, 0, 0, 0.8)
            bg = Rectangle(pos=container.pos, size=container.size)
        
        def update_bg(inst, val):
            bg.pos = inst.pos
            bg.size = inst.size
        
        container.bind(pos=update_bg, size=update_bg)
        
        # Label
        speed_val = coord.get("speed", 0)
        time_val = coord.get("timestamp", "?")
        
        info_label = Label(
            text=f"[b]Speed:[/b] {speed_val:.1f}\n[b]Time:[/b] {time_val}",
            markup=True,
            color=(1, 1, 1, 1),
            font_size="10sp"
        )
        
        container.add_widget(info_label)
        marker.add_widget(container)
        map_view.add_marker(marker)
        
        print(f"Created marker: Speed={speed_val:.1f}, Time={time_val}")


class LogMapApp(App):
    def build(self):
        sm = ScreenManager()
        sm.sessions = []
        sm.selected_session = None

        sm.add_widget(DragDropScreen(name="dragdrop"))
        sm.add_widget(SessionListScreen(name="session_list"))
        sm.add_widget(MapScreen(name="map"))
        
        return sm


if __name__ == "__main__":
    LogMapApp().run()