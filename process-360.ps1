mogrify -resize 1024x1024 *.png
mogrify -flip negy.png
mogrify -flop negy.png
mogrify -flip posy.png
mogrify -flop posy.png
ls *.png | %{ magick "$($_.basename).png" "$($_.basename).jpg"}
rm *.png
python $env:USERPROFILE\Programs\Cubemaps-to-Equirectangular-master\EquiFromTiles.py .
magick montage pythonOutput.png pythonOutput.png -tile 1x2 -geometry +0+0 final.png
