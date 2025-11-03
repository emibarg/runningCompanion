import os
from kivy.app import App
from kivy.uix.screenmanager import ScreenManager, Screen, FadeTransition
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.button import Button
from kivy.uix.label import Label
from kivy.graphics import Color, Line, Rectangle, RoundedRectangle, Ellipse
from kivy.core.window import Window
from kivy.clock import Clock
from kivy.utils import get_color_from_hex

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
    """Draws polyline on map with gradient effect."""
    
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
                # Draw shadow/outline
                Color(0.2, 0.2, 0.3, 0.4)
                Line(points=points, width=6, cap='round', joint='round')
                # Draw main line
                Color(0.2, 0.6, 1.0, 0.9)  # Nice blue color
                Line(points=points, width=4, cap='round', joint='round')


class CoordinateDotsLayer(MapLayer):
    """Draws dots for intermediate coordinates."""
    
    def __init__(self, coords, **kwargs):
        super().__init__(**kwargs)
        self.coords = coords

    def reposition(self):
        mapview = self.parent
        if not self.coords or mapview is None:
            return

        self.canvas.clear()
        
        # Draw dots for intermediate coordinates (skip first and last)
        for i, c in enumerate(self.coords):
            if i == 0 or i == len(self.coords) - 1:
                continue  # Skip start and finish
                
            try:
                x, y = mapview.get_window_xy_from(
                    lat=c["lat"], 
                    lon=c["lon"], 
                    zoom=mapview.zoom
                )
                
                with self.canvas:
                    # Outer circle (border)
                    Color(*get_color_from_hex('#0f3460'))
                    Ellipse(pos=(x - 6, y - 6), size=(12, 12))
                    # Inner circle (dot)
                    Color(*get_color_from_hex('#53d9ff'))
                    Ellipse(pos=(x - 4, y - 4), size=(8, 8))
            except:
                pass


class ClickableDot(MapMarkerPopup):
    """Invisible marker for intermediate coordinates that shows info on click."""
    
    def __init__(self, coord, **kwargs):
        super().__init__(lat=coord["lat"], lon=coord["lon"], **kwargs)
        self.coord = coord
        self.popup_showing = False
        
        # Make the marker invisible with larger clickable area
        self.source = ''
        self.size_hint = (None, None)
        self.size = (30, 30)  # Larger clickable area
        self.anchor_x = 0.5
        self.anchor_y = 0.5
        
        # Set background color to transparent
        self.color = [0, 0, 0, 0]
        self.background_color = [0, 0, 0, 0]
        
    def on_press(self):
        """Toggle popup on click."""
        if self.popup_showing:
            # Hide popup
            self.clear_widgets()
            self.popup_showing = False
        else:
            # Show popup
            self.show_popup()
            self.popup_showing = True
    
    def show_popup(self):
        """Create and show popup content."""
        # Create popup content
        container = BoxLayout(
            orientation="vertical",
            size_hint=(None, None),
            size=(110, 60),
            padding=[5, 5]
        )
        
        # Background with rounded corners
        with container.canvas.before:
            Color(*get_color_from_hex('#16213e'))
            bg = RoundedRectangle(pos=container.pos, size=container.size, radius=[10])
            # Border
            Color(*get_color_from_hex('#0f3460'))
            border = Line(rounded_rectangle=(
                container.x, container.y, 
                container.width, container.height, 10
            ), width=1.5)
        
        def update_bg(inst, val):
            bg.pos = inst.pos
            bg.size = inst.size
            border.rounded_rectangle = (inst.x, inst.y, inst.width, inst.height, 10)
        
        container.bind(pos=update_bg, size=update_bg)
        
        # Labels
        speed_val = self.coord.get("speed", 0)
        time_val = self.coord.get("timestamp", "?")
        
        speed_label = Label(
            text=f"[color=53d9ff][b]{speed_val:.1f}[/b][/color] km/h",
            markup=True,
            color=(1, 1, 1, 1),
            font_size="12sp",
            size_hint_y=0.5
        )
        
        time_label = Label(
            text=f"{time_val}",
            markup=True,
            color=get_color_from_hex('#e94560'),
            font_size="10sp",
            size_hint_y=0.5
        )
        
        container.add_widget(speed_label)
        container.add_widget(time_label)
        
        # Clear previous popup content and add new
        self.clear_widgets()
        self.add_widget(container)


class DragDropScreen(Screen):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        
        # Main layout with gradient background
        layout = BoxLayout(orientation='vertical', padding=40, spacing=20)
        
        with layout.canvas.before:
            Color(*get_color_from_hex('#1a1a2e'))
            self.bg_rect = Rectangle(pos=layout.pos, size=layout.size)
        
        layout.bind(pos=self._update_bg, size=self._update_bg)
        
        # Title
        title = Label(
            text="[b]Running Companion[/b]",
            markup=True,
            font_size='32sp',
            size_hint_y=None,
            height=80,
            color=get_color_from_hex('#ffffff')
        )
        
        # Drop zone
        drop_zone = BoxLayout(orientation='vertical', padding=60, spacing=20)
        with drop_zone.canvas.before:
            Color(*get_color_from_hex('#16213e'))
            self.drop_bg = RoundedRectangle(
                pos=drop_zone.pos, 
                size=drop_zone.size,
                radius=[20]
            )
        
        drop_zone.bind(pos=self._update_drop_bg, size=self._update_drop_bg)
        
        # Wave logo container
        wave_container = BoxLayout(size_hint_y=None, height=100)
        with wave_container.canvas:
            # Draw wave pattern
            Color(*get_color_from_hex('#53d9ff'))
            self.wave = Line(points=[], width=3, cap='round', joint='round')
        
        wave_container.bind(pos=self._update_wave, size=self._update_wave)
        Clock.schedule_once(lambda dt: self._update_wave(wave_container, None), 0.1)
        
        self.label = Label(
            text="Drop your log file here\n\n(.txt files only)",
            markup=True,
            halign="center",
            valign="middle",
            font_size='20sp',
            color=get_color_from_hex('#e94560')
        )
        
        drop_zone.add_widget(wave_container)
        drop_zone.add_widget(self.label)
        
        layout.add_widget(title)
        layout.add_widget(drop_zone)
        
        self.add_widget(layout)
        Window.bind(on_drop_file=self._on_file_drop)

    def _update_bg(self, instance, value):
        self.bg_rect.pos = instance.pos
        self.bg_rect.size = instance.size
    
    def _update_drop_bg(self, instance, value):
        self.drop_bg.pos = instance.pos
        self.drop_bg.size = instance.size
    
    def _update_wave(self, instance, value):
        """Draw a Morlet wavelet pattern."""
        import math
        
        # Wait until widget is properly sized
        if instance.width == 0 or instance.height == 0:
            return
            
        points = []
        center_x = instance.x + instance.width / 2
        center_y = instance.y + instance.height / 2
        width = min(instance.width * 0.6, 300)
        
        # Morlet wavelet parameters
        omega = 5.0  # Angular frequency
        sigma = 1.0  # Scaling factor
        amplitude = 25
        
        for i in range(150):
            t = (i - 75) / 15.0  # Center around 0, range from -5 to 5
            x = center_x - width/2 + (i * width / 150)
            
            # Morlet wavelet: cos(omega*t) * exp(-t^2 / (2*sigma^2))
            gaussian = math.exp(-(t**2) / (2 * sigma**2))
            wave = math.cos(omega * t) * gaussian
            y = center_y + amplitude * wave
            
            points.extend([x, y])
        
        self.wave.points = points

    def _on_file_drop(self, window, file_path_bytes, x, y):
        try:
            path = file_path_bytes.decode("utf-8")
        except:
            path = os.fsdecode(file_path_bytes)

        if not path.lower().endswith(".txt"):
            self.label.text = "[b]Please drop a .txt file[/b]"
            return

        sessions = parse_log_data(path)
        if sessions:
            self.manager.sessions = sessions
            self.manager.current = "session_list"
        else:
            self.label.text = "[b]Could not parse file[/b]\n\nPlease check the format"


class SessionListScreen(Screen):
    def on_pre_enter(self):
        self.clear_widgets()
        
        layout = BoxLayout(orientation="vertical", padding=20, spacing=15)
        
        with layout.canvas.before:
            Color(*get_color_from_hex('#1a1a2e'))
            self.bg = Rectangle(pos=layout.pos, size=layout.size)
        
        layout.bind(pos=self._update_bg, size=self._update_bg)
        
        # Header
        header = Label(
            text="[b]Your Sessions[/b]",
            markup=True,
            size_hint_y=None,
            height=60,
            font_size='28sp',
            color=get_color_from_hex('#ffffff')
        )
        layout.add_widget(header)

        if not getattr(self.manager, "sessions", None):
            layout.add_widget(Label(text="No sessions loaded", color=(1, 1, 1, 0.7)))
        else:
            for i, s in enumerate(self.manager.sessions):
                summary = s.get("summary", "")
                date = s.get("date", "?")
                time = s.get("time", "?")
                
                # Extract just the important info from summary
                summary_text = ""
                if summary and "Steps:" in summary:
                    parts = summary.split(",")
                    if len(parts) >= 2:
                        summary_text = f"{parts[0].split(':')[1].strip()}, {parts[1].strip()}"
                
                btn_container = BoxLayout(size_hint_y=None, height=90, padding=[5, 5])
                
                btn = Button(
                    text=f"[b]Session {i+1}[/b]\n{date} at {time}\n{summary_text}",
                    markup=True,
                    background_normal='',
                    background_color=get_color_from_hex('#0f3460'),
                    color=get_color_from_hex('#ffffff'),
                    font_size='13sp',
                    halign='center',
                    valign='middle',
                    padding=[10, 10]
                )
                btn.bind(size=btn.setter('text_size'))
                
                with btn.canvas.before:
                    Color(*get_color_from_hex('#16213e'))
                    btn.bg_rect = RoundedRectangle(pos=btn.pos, size=btn.size, radius=[15])
                
                btn.bind(pos=lambda inst, val, r=btn.bg_rect: setattr(r, 'pos', inst.pos))
                btn.bind(size=lambda inst, val, r=btn.bg_rect: setattr(r, 'size', inst.size))
                btn.bind(on_press=lambda inst, idx=i: self.open_session(idx))
                
                btn_container.add_widget(btn)
                layout.add_widget(btn_container)

        # Back button
        back_btn = Button(
            text="Back",
            size_hint_y=None,
            height=50,
            background_normal='',
            background_color=get_color_from_hex('#e94560'),
            color=get_color_from_hex('#ffffff'),
            font_size='16sp',
            bold=True
        )
        back_btn.bind(on_press=lambda b: setattr(self.manager, "current", "dragdrop"))
        layout.add_widget(back_btn)
        
        self.add_widget(layout)

    def _update_bg(self, instance, value):
        self.bg.pos = instance.pos
        self.bg.size = instance.size

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

        # Header with gradient
        header = BoxLayout(size_hint_y=None, height=70, padding=[10, 10], spacing=10)
        
        with header.canvas.before:
            Color(*get_color_from_hex('#16213e'))
            self.header_bg = Rectangle(pos=header.pos, size=header.size)
        
        header.bind(pos=self._update_header_bg, size=self._update_header_bg)
        
        session_info = Label(
            text=f"[b]{session.get('summary','')}[/b]",
            markup=True,
            size_hint_x=0.8,
            color=get_color_from_hex('#ffffff'),
            font_size='16sp'
        )
        
        back_btn = Button(
            text="Back",
            size_hint_x=0.2,
            background_normal='',
            background_color=get_color_from_hex('#e94560'),
            color=get_color_from_hex('#ffffff'),
            bold=True
        )
        back_btn.bind(on_press=lambda b: setattr(self.manager, "current", "session_list"))
        
        header.add_widget(session_info)
        header.add_widget(back_btn)

        # Create map
        map_view = MapView(zoom=15)
        
        Clock.schedule_once(
            lambda dt: map_view.center_on(coords[0]["lat"], coords[0]["lon"]), 
            0.1
        )

        # Add route line
        route_layer = RouteLayer(coords)
        map_view.add_layer(route_layer)
        
        # Add dots layer for intermediate coordinates
        dots_layer = CoordinateDotsLayer(coords)
        map_view.add_layer(dots_layer)
        
        # Force initial draw
        Clock.schedule_once(lambda dt: route_layer.reposition(), 0.2)
        Clock.schedule_once(lambda dt: dots_layer.reposition(), 0.2)

        # Add clickable invisible markers for intermediate coordinates
        for i, coord in enumerate(coords):
            if i == 0:
                # Start marker
                self.create_start_marker(map_view, coord)
            elif i == len(coords) - 1:
                # Finish marker
                self.create_finish_marker(map_view, coord)
            else:
                # Clickable dot (invisible marker)
                clickable_dot = ClickableDot(coord)
                map_view.add_marker(clickable_dot)

        main_layout.add_widget(header)
        main_layout.add_widget(map_view)
        self.add_widget(main_layout)

    def _update_header_bg(self, instance, value):
        self.header_bg.pos = instance.pos
        self.header_bg.size = instance.size
    
    def create_start_marker(self, map_view, coord):
        """Create START marker."""
        marker = MapMarkerPopup(lat=coord["lat"], lon=coord["lon"])
        
        container = BoxLayout(
            orientation="vertical",
            size_hint=(None, None),
            size=(120, 80),
            padding=[5, 5]
        )
        
        with container.canvas.before:
            Color(*get_color_from_hex('#16213e'))
            bg = RoundedRectangle(pos=container.pos, size=container.size, radius=[10])
            Color(*get_color_from_hex('#00ff00'))
            border = Line(rounded_rectangle=(
                container.x, container.y, 
                container.width, container.height, 10
            ), width=2)
        
        def update_bg(inst, val):
            bg.pos = inst.pos
            bg.size = inst.size
            border.rounded_rectangle = (inst.x, inst.y, inst.width, inst.height, 10)
        
        container.bind(pos=update_bg, size=update_bg)
        
        start_label = Label(
            text="[color=00ff00][b]START[/b][/color]",
            markup=True,
            color=(1, 1, 1, 1),
            font_size="14sp",
            size_hint_y=0.4
        )
        
        speed_val = coord.get("speed", 0)
        time_val = coord.get("timestamp", "?")
        
        speed_label = Label(
            text=f"[color=53d9ff][b]{speed_val:.1f}[/b][/color] km/h",
            markup=True,
            color=(1, 1, 1, 1),
            font_size="11sp",
            size_hint_y=0.3
        )
        
        time_label = Label(
            text=f"{time_val}",
            markup=True,
            color=get_color_from_hex('#e94560'),
            font_size="10sp",
            size_hint_y=0.3
        )
        
        container.add_widget(start_label)
        container.add_widget(speed_label)
        container.add_widget(time_label)
        marker.add_widget(container)
        map_view.add_marker(marker)
    
    def create_finish_marker(self, map_view, coord):
        """Create FINISH marker."""
        marker = MapMarkerPopup(lat=coord["lat"], lon=coord["lon"])
        
        container = BoxLayout(
            orientation="vertical",
            size_hint=(None, None),
            size=(120, 80),
            padding=[5, 5]
        )
        
        with container.canvas.before:
            Color(*get_color_from_hex('#16213e'))
            bg = RoundedRectangle(pos=container.pos, size=container.size, radius=[10])
            Color(*get_color_from_hex('#ff0000'))
            border = Line(rounded_rectangle=(
                container.x, container.y, 
                container.width, container.height, 10
            ), width=2)
        
        def update_bg(inst, val):
            bg.pos = inst.pos
            bg.size = inst.size
            border.rounded_rectangle = (inst.x, inst.y, inst.width, inst.height, 10)
        
        container.bind(pos=update_bg, size=update_bg)
        
        finish_label = Label(
            text="[color=ff0000][b]FINISH[/b][/color]",
            markup=True,
            color=(1, 1, 1, 1),
            font_size="14sp",
            size_hint_y=0.4
        )
        
        speed_val = coord.get("speed", 0)
        time_val = coord.get("timestamp", "?")
        
        speed_label = Label(
            text=f"[color=53d9ff][b]{speed_val:.1f}[/b][/color] km/h",
            markup=True,
            color=(1, 1, 1, 1),
            font_size="11sp",
            size_hint_y=0.3
        )
        
        time_label = Label(
            text=f"{time_val}",
            markup=True,
            color=get_color_from_hex('#e94560'),
            font_size="10sp",
            size_hint_y=0.3
        )
        
        container.add_widget(finish_label)
        container.add_widget(speed_label)
        container.add_widget(time_label)
        marker.add_widget(container)
        map_view.add_marker(marker)


class LogMapApp(App):
    def build(self):
        Window.clearcolor = get_color_from_hex('#1a1a2e')
        
        sm = ScreenManager(transition=FadeTransition())
        sm.sessions = []
        sm.selected_session = None

        sm.add_widget(DragDropScreen(name="dragdrop"))
        sm.add_widget(SessionListScreen(name="session_list"))
        sm.add_widget(MapScreen(name="map"))
        
        return sm


if __name__ == "__main__":
    LogMapApp().run()