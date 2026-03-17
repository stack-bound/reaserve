// ReaServe Go client example.
//
// Usage:
//
//	go run go_client.go
package main

import (
	"encoding/binary"
	"encoding/json"
	"fmt"
	"net"
	"sync/atomic"
)

type Client struct {
	conn   net.Conn
	nextID atomic.Uint64
}

type rpcRequest struct {
	JSONRPC string      `json:"jsonrpc"`
	ID      uint64      `json:"id"`
	Method  string      `json:"method"`
	Params  interface{} `json:"params"`
}

type rpcResponse struct {
	JSONRPC string          `json:"jsonrpc"`
	ID      uint64          `json:"id"`
	Result  json.RawMessage `json:"result,omitempty"`
	Error   *rpcError       `json:"error,omitempty"`
}

type rpcError struct {
	Code    int    `json:"code"`
	Message string `json:"message"`
}

func NewClient(addr string) (*Client, error) {
	conn, err := net.Dial("tcp", addr)
	if err != nil {
		return nil, err
	}
	return &Client{conn: conn}, nil
}

func (c *Client) Close() {
	c.conn.Close()
}

func (c *Client) Call(method string, params interface{}) (json.RawMessage, error) {
	if params == nil {
		params = map[string]interface{}{}
	}

	id := c.nextID.Add(1)
	req := rpcRequest{JSONRPC: "2.0", ID: id, Method: method, Params: params}
	data, err := json.Marshal(req)
	if err != nil {
		return nil, err
	}

	// Write length-prefixed frame
	header := make([]byte, 4)
	binary.BigEndian.PutUint32(header, uint32(len(data)))
	if _, err := c.conn.Write(header); err != nil {
		return nil, err
	}
	if _, err := c.conn.Write(data); err != nil {
		return nil, err
	}

	// Read response
	if _, err := c.conn.Read(header); err != nil {
		return nil, err
	}
	length := binary.BigEndian.Uint32(header)
	buf := make([]byte, length)
	n := uint32(0)
	for n < length {
		read, err := c.conn.Read(buf[n:])
		if err != nil {
			return nil, err
		}
		n += uint32(read)
	}

	var resp rpcResponse
	if err := json.Unmarshal(buf, &resp); err != nil {
		return nil, err
	}
	if resp.Error != nil {
		return nil, fmt.Errorf("RPC error %d: %s", resp.Error.Code, resp.Error.Message)
	}
	return resp.Result, nil
}

func main() {
	client, err := NewClient("localhost:9876")
	if err != nil {
		fmt.Printf("Connect failed: %v\n", err)
		return
	}
	defer client.Close()

	// Ping
	result, err := client.Call("ping", nil)
	if err != nil {
		fmt.Printf("Ping failed: %v\n", err)
		return
	}
	fmt.Printf("Ping: %s\n", result)

	// Get project state
	result, err = client.Call("project.get_state", nil)
	if err != nil {
		fmt.Printf("Get state failed: %v\n", err)
		return
	}

	var state map[string]interface{}
	json.Unmarshal(result, &state)
	fmt.Printf("\nProject: %.0f BPM, %.0f tracks\n", state["bpm"], state["track_count"])

	// Add a track
	result, err = client.Call("track.add", map[string]interface{}{"name": "New Track"})
	if err != nil {
		fmt.Printf("Add track failed: %v\n", err)
		return
	}
	fmt.Printf("\nAdded track: %s\n", result)
}
