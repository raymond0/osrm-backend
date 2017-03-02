@routing @speed @traffic
Feature: Traffic - speeds

    Background: Use specific speeds
        Given the node locations
          | node |   lat |   lon |
          | a    |   0.1 |   0.1 |
          | b    |  0.05 |   0.1 |
          | c    |   0.0 |   0.1 |
          | d    |  0.05 |  0.03 |
          | e    |  0.05 | 0.066 |
          | f    | 0.075 | 0.066 |
          | g    | 0.075 |   0.1 |
        And the ways
          | nodes | highway |
          | ab    | primary |
          | ad    | primary |
          | bc    | primary |
          | dc    | primary |
          | de    | primary |
          | eb    | primary |
          | df    | primary |
          | fb    | primary |
        And the profile "testbot"
        And the extract extra arguments "--generate-edge-lookup"


    Scenario: Weighting based on speed file
        Given the contract extra arguments "--segment-speed-file {speeds_file}"
        Given the speed file
        """
        1,2,0
        2,1,0
        2,3,27
        3,2,27
        1,4,27
        4,1,27
        """
        And I route I should get
          | from | to | route       | speed   | weights              |
          | a    | b  | ad,de,eb,eb | 30 km/h | 1275.7,400.4,378.2,0 |
          | a    | c  | ad,dc,dc    | 31 km/h | 1275.7,956.8,0       |
          | b    | c  | bc,bc       | 27 km/h | 741.5,0              |
          | a    | d  | ad,ad       | 27 km/h | 1275.7,0             |
          | d    | c  | dc,dc       | 36 km/h | 956.8,0              |
          | g    | b  | fb,fb       | 36 km/h | 164.7,0              |
          | a    | g  | ad,df,fb,fb | 30 km/h | 1275.7,487.5,304.7,0 |


    Scenario: Weighting based on speed file weights, ETA based on file durations
        Given the contract extra arguments "--segment-speed-file {speeds_file}"
        Given the speed file
        """
        1,2,1,20020.7
        2,1,1,20020.7
        2,3,27,741.5
        3,2,27,741.5
        1,4,27,1275.7
        4,1,27,1275.7
        """
        And I route I should get
          | from | to | route       | speed   | weights              |
          | a    | b  | ad,de,eb,eb | 30 km/h | 1275.7,400.4,378.2,0 |
          | a    | c  | ad,dc,dc    | 31 km/h | 1275.7,956.8,0       |
          | b    | c  | bc,bc       | 27 km/h | 741.5,0              |
          | a    | d  | ad,ad       | 27 km/h | 1275.7,0             |
          | d    | c  | dc,dc       | 36 km/h | 956.8,0              |
          | g    | b  | ab,ab       | 1 km/h  | 10010.4,0            |
          | a    | g  | ab,ab       | 1 km/h  | 10010.3,0            |


    Scenario: Weighting based on speed file weights, ETA based on file durations
        Given the profile file "testbot" extended with
        """
        api_version = 1
        properties.traffic_signal_penalty = 0
        properties.u_turn_penalty = 0
        properties.weight_precision = 3
        """
        And the contract extra arguments "--segment-speed-file {speeds_file}"
        Given the speed file
        """
        1,2,1,20020.789
        2,1,1,20020.123
        2,3,27,741.56789
        3,2,27,741.3
        1,4,1,34445.12
        4,1,1,34445.3
        """
        And I route I should get
          | from | to | route       | speed   | weights                     |
          | a    | b  | ab,ab       | 1 km/h  | 20020.789,0                 |
          | a    | c  | ab,bc,bc    | 2 km/h  | 20020.789,741.568,0         |
          | b    | c  | bc,bc       | 27 km/h | 741.568,0                   |
          | a    | d  | ab,eb,de,de | 2 km/h  | 20020.789,378.169,400.415,0 |
          | d    | c  | dc,dc       | 36 km/h | 956.805,0                   |
          | g    | b  | ab,ab       | 1 km/h  | 10010.392,0                 |
          | a    | g  | ab,ab       | 1 km/h  | 10010.397,0                 |
          | g    | a  | ab,ab       | 1 km/h  | 10010.064,0                 |


    Scenario: Speeds that isolate a single node (a)
        Given the contract extra arguments "--segment-speed-file {speeds_file}"
        And the node locations
          | node | lat        | lon      |
          | h    | 2.075      | 19.1     |
        Given the speed file
        """
        1,2,0
        2,1,0
        2,3,27
        3,2,27
        1,4,0
        4,1,0
        """
        And I route I should get
          | from | to | route    | speed   | weights       |
          | a    | b  | fb,fb    | 36 km/h | 329.4,0       |
          | a    | c  | fb,bc,bc | 30 km/h | 329.4,741.5,0 |
          | b    | c  | bc,bc    | 27 km/h | 741.5,0       |
          | a    | d  | fb,df,df | 36 km/h | 140,487.5,0   |
          | d    | c  | dc,dc    | 36 km/h | 956.8,0       |
          | g    | b  | fb,fb    | 36 km/h | 164.7,0       |
          | a    | g  | fb,fb    | 36 km/h | 164.7,0       |


    Scenario: Verify that negative values cause an error, they're not valid at all
        Given the speed file
        """
        1,2,-10
        2,1,-20
        2,3,27
        3,2,27
        1,4,-3
        4,1,-5
        """
        And the data has been extracted
        When I try to run "osrm-contract --segment-speed-file {speeds_file} {processed_file}"
        And stderr should contain "malformed"
        And it should exit with an error
