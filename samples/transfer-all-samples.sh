for filepath in $(ls factory_kit/*.wav | sort); do
    filename=$(basename "$filepath")
    index=$(echo "$filename" | grep -o '^[0-9]*')
    ../tools/drumtool/drumtool.js send "$filepath:$index"
done
