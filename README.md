# Shader Dock

*A native OBS dock that finds your shader, 3D and perspective filters and gives each one play, progress and drag controls.*

Dock nativo para **OBS Studio** (C++ y Qt) que encuentra en todas tus escenas y fuentes los filtros de shader de mis repos y los filtros de geometría, y te da controles para cada uno sin abrir sus propiedades. Es la versión plugin de [obs-shader-dock](https://github.com/hjmeneses/obs-shader-dock): no necesita WebSocket ni contraseña, y toma los colores del tema de OBS.

*[English below](#english)*

## Descarga e instalación

Descarga el último [release](https://github.com/hjmeneses/shader-dock/releases/latest) (compilado para OBS 33):

- **Windows:** con OBS cerrado, descomprime el `.zip` en `C:\ProgramData\obs-studio\plugins`.
- **Ubuntu:** `sudo apt install ./shader-dock-<versión>-x86_64-linux-gnu.deb`.
- **macOS:** instala el `.pkg`.

Abre OBS y activa el dock en **Paneles → Shaders**. Si usabas el dock de navegador, puedes quitarlo: este lo reemplaza.

Los filtros que controla vienen de sus propios plugins y repos: [obs-shaderfilter](https://github.com/exeldro/obs-shaderfilter) con los shaders de obs-loving-spies, obs-retro-tv, obs-kr36, obs-liquid-glass y obs-modern-tv; [Light Trace](https://github.com/hjmeneses/light-trace); [3D Effect](https://github.com/exeldro/obs-3d-effect) y [3D Effect 2](https://github.com/hjmeneses/obs-3d-effect/tree/effect-2); y BFX 3D Rotation, el script que viene con OBS.

## Pestaña Efectos

- **Secuencias** (cañón, sangre, túnel, títulos voladores, trazo de luz…): ▶ corre la secuencia una vez en su duración y el mismo botón la detiene; ⏮ y ⏭ llevan el progreso a 0 o a 100 %; una barra para moverlo a mano y la casilla **Bucle**.
- **Efectos continuos** (liquid glass, glitch…): interruptor para encender y apagar.

## Pestaña Geometría

Cada filtro trae un pad que se arrastra con el mouse sobre una captura de la fuente, tal como sale al aire. La captura se toma directo de OBS y se actualiza mientras arrastras.

- **3D Effect y 3D Effect 2:** arrastrar gira en X/Y; **Shift** gira en Z; **Ctrl** mueve; la rueda acerca o aleja; las flechas giran de 5° en 5°. Los botones X, Y y Z regresan ese eje a 0 y ↺ lo regresa todo (también con doble clic). La tarjeta chica de la esquina muestra la orientación, con una marca para saber si quedó de espaldas.
- **BFX 3D Rotation:** arrastrar gira en X/Y; en el modo de eje, el arrastre horizontal gira y el vertical cambia el ángulo del eje; la rueda cambia la escala. El pad deja ver lo que se sale del recuadro de la fuente (marcado con línea punteada), porque BFX no recorta.
- **Perspectiva** (`perspectiva.shader`): cuatro anclas sobre una rejilla en perspectiva. **Shift** mueve la esquina vecina en espejo, arrastrar adentro o con **Alt** mueve todo, y ⤢ activa o quita el margen (Expandir) del filtro para que la imagen pueda salirse del recuadro.

Si mueves los sliders en las propiedades del filtro, el pad se pone al día.

## Lista

- Las tarjetas se agrupan por fuente. Con **Solo escena en vivo** apagado, las fuentes al aire van primero y marcadas en rojo; las de la vista previa (modo estudio), en azul.
- **Solo escena en vivo** muestra solo la escena al aire, con sus grupos y escenas anidadas a cualquier profundidad. Si no tiene filtros, una tarjeta lo dice y su botón **Agregar filtro…** abre los filtros de la fuente seleccionada.
- **Mostrar otros shaders** incluye los *User-defined shader* que no son de estos repos.
- La lista se actualiza sola al agregar, quitar o renombrar filtros, fuentes y escenas. El dock recuerda la pestaña y las casillas.
- Textos en español e inglés, según el idioma de OBS.

## Agregar un shader

La lista de shaders reconocidos es `data/shaders.json`, generada a partir del `shaders.js` de [obs-shader-dock](https://github.com/hjmeneses/obs-shader-dock):

```
node tools/manifest.mjs
```

Un filtro nativo se reconoce por su id (campo `kinds`); un *User-defined shader*, por el nombre de su archivo.

## Compilar

Igual que la [plantilla oficial de plugins de OBS](https://github.com/obsproject/obs-plugintemplate), con Qt: `cmake --preset windows-x64` y `cmake --build build_x64 --config RelWithDebInfo`.

---

## English

A native **OBS Studio** dock (C++ / Qt) that scans every scene and source for the shader filters from my repos and for geometry filters, and gives each one controls without opening its properties. It is the plugin version of the browser dock [obs-shader-dock](https://github.com/hjmeneses/obs-shader-dock): no WebSocket or password needed, and it follows the OBS theme colours.

**Download:** [latest release](https://github.com/hjmeneses/shader-dock/releases/latest) (built for OBS 33). Windows: with OBS closed, extract the zip into `C:\ProgramData\obs-studio\plugins`. Ubuntu: `sudo apt install ./shader-dock-<version>-x86_64-linux-gnu.deb`. macOS: install the `.pkg`. Then turn it on under **Docks → Shaders**.

- **Effects tab.** Sequences: ▶ plays once over the filter's duration (and stops it), ⏮ / ⏭ jump to 0 or 100 %, a progress bar and a **Loop** box. Continuous effects: an on/off switch.
- **Geometry tab.** A drag pad over a live capture of the source:
  - **3D Effect / 3D Effect 2:** drag rotates X/Y, **Shift** rotates Z, **Ctrl** moves, the wheel zooms, arrows step 5°. X, Y, Z reset one axis; ↺ or double-click resets everything.
  - **BFX 3D Rotation:** drag rotates X/Y (in axis mode, horizontal rotates and vertical turns the axis); the wheel scales. Overflow past the source bounds is shown, since BFX does not clip.
  - **Perspective** (`perspective.shader`): four anchors over a perspective grid; **Shift** mirrors the neighbouring corner, drag inside or **Alt** moves everything, ⤢ toggles the filter's Expand margin.
- **Live scene only** lists just the scene on air, including groups and nested scenes. When it has none, a card says so and **Add filter…** opens the filters of the selected source. **Show other shaders** includes *User-defined shader* filters that are not from these repos.

Filters come from their own plugins: [obs-shaderfilter](https://github.com/exeldro/obs-shaderfilter) with the shaders from my repos, [Light Trace](https://github.com/hjmeneses/light-trace), [3D Effect](https://github.com/exeldro/obs-3d-effect), [3D Effect 2](https://github.com/hjmeneses/obs-3d-effect/tree/effect-2) and the BFX 3D Rotation script that ships with OBS. The list of known shaders is `data/shaders.json`, generated from obs-shader-dock's `shaders.js` with `node tools/manifest.mjs`.

## License

GPL-2.0-or-later
