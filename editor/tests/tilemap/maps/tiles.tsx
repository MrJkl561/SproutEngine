<?xml version="1.0" encoding="UTF-8"?>
<tileset version="1.10" tiledversion="1.11.0" name="tiles" tilewidth="16" tileheight="16" tilecount="4" columns="4">
 <image source="art/tiles.png" width="64" height="16"/>
 <tile id="0">
  <properties>
   <property name="solid" type="bool" value="true"/>
  </properties>
 </tile>
 <tile id="1" class="spikes"/>
 <tile id="2">
  <objectgroup draworder="index" id="2">
   <object id="1" x="0" y="0" width="16" height="16"/>
  </objectgroup>
 </tile>
</tileset>
