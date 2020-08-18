import React, { Component } from "react";
import * as d3 from "d3";
import * as Stardust from "stardust-core";
import * as THREE from 'three';
import { color } from "d3";
// import png from "circle.png"
class Graph extends Component {


  
  componentWillReceiveProps(props) {
    if (this.props.graph == props.graph || props.graph.links.length == 0) {
      return;
    }

    const graph = props.graph;

    console.log(graph.links.length)

    const parent_ele = document.getElementById("graph");
    parent_ele.innerHTML = ""
    const width = parent_ele.clientWidth - 10;
    const height = document.body.clientHeight - 10;

    // Add canvas
    let renderer = new THREE.WebGLRenderer();
    renderer.setSize(width, height);
    parent_ele.appendChild(renderer.domElement);

    const near_plane = 2;
    const far_plane = 100;

    // Set up camera and scene
    let camera = new THREE.PerspectiveCamera(
      20,
      width / height,
      near_plane,
      far_plane 
    );
    camera.position.set(0, 0, far_plane);
    camera.lookAt(new THREE.Vector3(0,0,0));
    const scene = new THREE.Scene();
    scene.background = new THREE.Color(0xffffff);

    // let circle_sprite_aa= new THREE.TextureLoader().load(
    //   "https://blog.fastforwardlabs.com/images/2018/02/circle_aa-1518730700478.png"
    // )

    // let sprite_settings = {
    //   map: circle_sprite_aa,
    //   transparent: true,
    //   alphaTest: 0.5
    // }

    // const pointsGeometry = new THREE.Geometry();
    // const colors = [];
    // for (const point of graph.nodes) {
    //   const vertex = new THREE.Vector3(point.x, point.y, 0);
    //   pointsGeometry.vertices.push(vertex);
    //   const color = new THREE.Color();
    //   color.setHSL(0, 0, 0);
    //   colors.push(color);
    // }
    // pointsGeometry.colors = colors;
    // let pointsMaterial = new THREE.PointsMaterial({
    //   // map: spriteMap,
    //   size: 3,
    //   // transparent: true,
    //   // blending: THREE.AdditiveBlending,
    //   sizeAttenuation: false,
    //   vertexColors: THREE.VertexColors,
    // });

    // for (let setting in sprite_settings) {
    //   pointsMaterial[setting] = sprite_settings[setting];
    // }

    // const points = new THREE.Points(pointsGeometry, pointsMaterial);
    // const pointsContainer = new THREE.Object3D();
    // pointsContainer.add(points);
    
    
    function piecewise(interpolate, values) {
      var i = 0, n = values.length - 1, v = values[0], I = new Array(n < 0 ? 0 : n);
      while (i < n) I[i] = interpolate(v, v = values[++i]);
      return function(t) {
        var i = Math.max(0, Math.min(n - 1, Math.floor(t *= n)));
        return I[i](t - i);
      };
    }
    let interpolateJet = function(){
      //The steps in the jet colorscale
      const jet_data_lin = [
        [0,0,0.5],
        [0,0,1],
        [0,0.5,1],
        [0,1,1],
        [0.5,1,0.5],
        [1,1,0],
        [1,0.5,0],
        [1,0,0],
        [0.5,0,0]
      ]

      jet_data_lin.reverse();
      
      const jet_rgb = jet_data_lin.map(x => {
        return d3.rgb.apply(null, x.map(y=>y*255))
      })
      
      //perform piecewise interpolation between each color in the range
      return piecewise(d3.interpolateRgb, jet_rgb)
    }()

    const scale_mode = 15

    let edges = []
    let distance_link = []
    let links_colors = []
    for(let i = 0; i < graph.links.length; ++i) {
      let edge = graph.links[i]
      distance_link.push( Math.sqrt( (edge.source_x - edge.target_x) ** 2 +  (edge.source_y - edge.target_y) ** 2 ))
      edges.push(new THREE.Vector3(edge.source_x * scale_mode, edge.source_y  * scale_mode, 0 ))
      edges.push(new THREE.Vector3(edge.target_x * scale_mode, edge.target_y  * scale_mode, 0 ))
    }

    
    let min_distance = distance_link[0]
    for(let i = 0; i < distance_link.length; ++i) {
      min_distance = Math.min(min_distance, distance_link[i])
    }

    for(let i = 0; i < distance_link.length; ++i) {
      distance_link[i] -= min_distance
    }

    let max_distance = distance_link[0]
    for(let i = 0; i < distance_link.length; ++i) {
      max_distance = Math.max(max_distance, distance_link[i])
    }

    max_distance = Math.max(0.1, max_distance)

    for(let i = 0; i < distance_link.length; ++i) {
      distance_link[i] /= max_distance
      let color_result = interpolateJet(distance_link[i])
      let [a, b, c] = color_result.substr(4, color_result.length - 5).split(",").map((x) => { return parseInt(x)})
      links_colors.push(a / 255, b / 255, c / 255)
      links_colors.push(a / 255, b / 255, c / 255)
    }

    let LineGeometry = new THREE.BufferGeometry().setFromPoints( edges );
    
    LineGeometry.setAttribute( 'color', new THREE.Float32BufferAttribute( links_colors, 3 ) );

    let LineMaterial  = new THREE.LineBasicMaterial( { vertexColors: true, morphTargets: true  } );

    let line = new THREE.LineSegments( LineGeometry, LineMaterial );

    scene.add( line );
    // scene.add(pointsContainer);

    // Set up zoom behavior
    const zoom = d3.zoom()
      .scaleExtent([20, far_plane])
      .wheelDelta(function wheelDelta() {
        // this inverts d3 zoom direction, which makes it the rith zoom direction for setting the camera
        return d3.event.deltaY * (d3.event.deltaMode ? 120 : 1) / 500;
      })
      .on('zoom', () => {
        const event = d3.event;
        if (event.sourceEvent) {

          // Get z from D3
          const new_z = event.transform.k;
          // console.log(new_z)
          if(new_z < 20) return

          if (new_z !== camera.position.z) {
            
            // Handle a zoom event
            const { clientX, clientY } = event.sourceEvent;

            // Project a vector from current mouse position and zoom level
            // Find the x and y coordinates for where that vector intersects the new
            // zoom level.
            // Code from WestLangley https://stackoverflow.com/questions/13055214/mouse-canvas-x-y-to-three-js-world-x-y-z/13091694#13091694
            const vector = new THREE.Vector3(
              clientX / width * 2 - 1,
              - (clientY / height) * 2 + 1,
              1 
            );
            vector.unproject(camera);
            const dir = vector.sub(camera.position).normalize();
            const distance = (new_z - camera.position.z)/dir.z;
            const pos = camera.position.clone().add(dir.multiplyScalar(distance));
            
            
            // if (camera.position.z < 20) {
            //   scale = (20 -  camera.position.z)/camera.position.z;
            //   pointsMaterial.setValues({size: 6 + 3 * scale});
            // } else if (camera.position.z >= 20 && pointsMaterial.size !== 6) {
            //   pointsMaterial.setValues({size: });
            // }
                            
            // Set the camera to new coordinates
            camera.position.set(pos.x, pos.y, new_z);

          } else {

            // Handle panning
            const { movementX, movementY } = event.sourceEvent;

            // Adjust mouse movement by current scale and set camera
            const current_scale = getCurrentScale();
            camera.position.set(camera.position.x - movementX/current_scale, camera.position.y +
              movementY/current_scale, camera.position.z);
          }
        }
      });

    // Add zoom listener
    const view = d3.select(renderer.domElement);
    view.call(zoom);
      
    // Disable double click to zoom because I'm not handling it in Three.js
    view.on('dblclick.zoom', null);

    // Sync d3 zoom with camera z position
    zoom.scaleTo(view, far_plane);

    // Three.js render loop
    function animate() {
      requestAnimationFrame(animate);
      renderer.render(scene, camera);
    }
    animate();

    // From https://github.com/anvaka/three.map.control, used for panning
    function getCurrentScale() {
      var vFOV = camera.fov * Math.PI / 180
      var scale_height = 2 * Math.tan( vFOV / 2 ) * camera.position.z
      var currentScale = height / scale_height
      return currentScale
    }
  }
  render() {
    return <div id="graph"></div>;
  }
}

export default Graph;
