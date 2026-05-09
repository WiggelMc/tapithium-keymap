{
  writeShellApplication,
  keymap-drawer,
  yq-go,
  chromium,
}:
# Draw SVG images of the keymap
writeShellApplication {
  name = "draw";
  runtimeInputs = [
    yq-go
    keymap-drawer
    chromium
  ];
  text = ''
    set +e

    out=./img
    keymap_dir=./config

    cmd() {
      keymap --config "$keymap_dir"/keymap_drawer.yaml "$@"
    }

    for file in "$keymap_dir"/*.keymap
    do
      name="$(basename --suffix=".keymap" "$file")"
      config="$out/$name.yaml"
      draw_html="$keymap_dir/$name.draw.html"
      echo "Found $name keymap"

      echo "- Removing old images"
      rm "$out"/"$name".yaml
      rm "$out"/"$name".svg
      rm "$out"/"$name"_*.svg

      echo "- Parsing keymap-drawer"
      cmd  parse --zmk-keymap "$file" > "$config"

      echo "- Drawing all layers"
      cmd draw "$config" > "$out"/"$name".svg

      layers=$(yq '.layers | keys | .[]' "$config")
      for layer in $layers
      do
        echo "- Drawing $layer layer"
        cmd draw "$config" --select-layers "$layer" > "$out"/"$name"_"$layer".svg
      done



      echo "- Creating PDF Hash"

      pdf_hash=$(cat "$out"/"$name".svg "$draw_html" | sha256sum | cut -d' ' -f1)

      if [ -f "$out/$name.pdf" ] && [ -f "$out/$name.pdf.hash" ] && [ "$(cat "$out/$name.pdf.hash")" = "$pdf_hash" ]; then
        echo "- Skipped Creating PDF"
      else
        echo "- Creating PDF"

        echo "$pdf_hash" > "$out/$name.pdf.hash"
        rm "$out"/"$name".pdf

        chromium --headless \
          --disable-gpu \
          --disable-features=VaapiVideoDecoder,UseChromeOSDirectVideoDecoder,VaapiOnNvidiaGPUs \
          --password-store=basic \
          --disable-software-rasterizer \
          --use-gl=swiftshader \
          --no-pdf-header-footer \
          --print-to-pdf="$out"/"$name".pdf \
          "file://$(realpath "$draw_html")" \
          2> >(grep -v libva >&2)
      fi

    done
  '';
}
