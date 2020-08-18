import React from "react";
import "./App.css";
import { Col, Row } from "antd";
import { Button, Select, Tooltip} from "antd";
const { Option } = Select;
import Graph from "./webgl-graph"
import axios from "axios";

class App extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      snapshots: [],
      graph: {},
      graph_file_list: []
    };

    this.graph_file = ""
  }


  handleLayout() {
    if(this.graph_file == "") {
      alert("you should choose the file name!")
      return
    }
    axios
    .post(
      "http://localhost:5000/getLayout",
      { file: this.graph_file },
      { Accept: "application/json", "Content-Type": "application/json" }
    )
    .then((response) => {
      this.setState({ graph: response.data });
    });
  }

  componentDidMount() {
    axios
      .get("http://localhost:5000/getAll", {
        Accept: "application/json",
        "Content-Type": "application/json",
      })
      .then((response) => {
        console.log(response.data["graph_list"])
        this.setState({
          graph: response.data["layout"],
          graph_file_list: response.data["graph_list"]
        });
      });
  }

  render() {
    const { graph, graph_file_list } = this.state;

    function handleChange(value) {
        this.graph_file = value
    }

    return (
      <div className="App">
          <Row span={2} style={{ margin: "10px" }}>
            <Col span={3}>
            <Tooltip title="your graph file and vec file should be located in the system/backend/data folder" >
              <span>Choose Your Graph File</span>
            </Tooltip>
            </Col>
            <Col span={3}>
              <Select
                defaultValue="Not choosed"
                style={{ width: 120 }}
                onChange={handleChange.bind(this)}
              >
                {graph_file_list.map((path, i) => { return (<Option value={path} key={i}>{path}</Option>); })}
              </Select>
            </Col>
            <Col span={3}>
            <Button type="primary" icon="search" onClick={this.handleLayout.bind(this)}>
            Layout
            </Button>
            </Col>
          </Row>
          <Row span={22}>
            <Graph
              graph={graph}
            />
          </Row>
      </div>
    );
  }
}

export default App;
