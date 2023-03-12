import javax.swing.*;
import java.awt.*;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.Socket;


class FollowThread extends Thread {
    public FollowThread(Socket connection, JLabel bid, JLabel offer, JLabel res) {
        this.connection = connection;
        this.bid = bid;
        this.offer = offer;
        this.res = res;
    }
    public void run() {
        try {
            BufferedReader out =
                    new BufferedReader(new InputStreamReader(connection.getInputStream()));
            while (true) {
                String data = out.readLine();
                String[] cmd = data.split(" ");
                if (cmd[0].equals("upd") && cmd.length == 3) {
                    synchronized (this) {
                        this.bid_i = Integer.parseInt(cmd[1].trim());
                        this.offer_i = Integer.parseInt(cmd[2].trim());
                    }

                    bid.setText("BID: " + cmd[1].trim());
                    offer.setText("OFFER: " + cmd[2].trim());
                }
                if (cmd[0].equals("res")) {
                    res.setText("Status: " + data);
                }
                System.out.println(data);
            }
        } catch (IOException e) {
            throw new RuntimeException(e);
        }

    }

    public synchronized int GetBidInt() {
        return bid_i;
    }

    public synchronized int GetOfferInt() {
        return offer_i;
    }

    private int bid_i;
    private int offer_i;
    private Socket connection;
    private JLabel bid;
    private JLabel offer;
    private JLabel res;
}

class Frame extends JFrame {
    public Frame(int width, int height) throws IOException {
        super();
        Socket conn = new Socket("127.0.0.1", 8080);
        JLabel bid = new JLabel("BID: ");
        JLabel offer = new JLabel("OFFER: ");
        JLabel status = new JLabel("Status: ");

        PrintWriter sender =
                new PrintWriter(conn.getOutputStream(), true);


        FollowThread th = new FollowThread(conn, bid, offer, status);
        th.start();

        JPanel listPane = new JPanel();
        listPane.setLayout(new BoxLayout(listPane, BoxLayout.PAGE_AXIS));


        setTitle("Trading GUI");
        setSize(new Dimension(width, height));


        JButton buy = new JButton("BUY");
        JButton sell = new JButton("SELL");

        buy.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                sender.println("buy " + th.GetOfferInt());
            }
        });

        sell.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
//                System.out.println("click");
                sender.println("sell " + th.GetBidInt());
            }
        });

        listPane.add(bid);
        listPane.add(offer);
        listPane.add(buy);
        listPane.add(sell);
        listPane.add(status);

        getContentPane().add(listPane);
    }
}


public class Main {
    public static void main(String[] args) throws IOException {
        Frame jf = new Frame(400, 400);
        jf.show();
    }
}
