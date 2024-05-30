#!/usr/bin/perl
#**********************************************************************
# Script pour generer les emails de status MARGIN ACTIVITY (COLLAT)   *
# *********************************************************************
#**********************************************************************
#* Mode de lancement: Par VTOM                                        *
#**********************************************************************
#* script appelant: R.A.S                                             *
#**********************************************************************
#**********************************************************************
#* Auteur: ADE                                                        *
#* Date de creation: 29/05/2024                                       *
#**********************************************************************
use lib "$ENV{STKROOT}/batch/";
use strict;
use warnings;
use Infra;
use POSIX qw(strftime);
use MIME::Base64;
use Net::SMTP;
use DBI;

#**********************************************************************
#* Variables                                                          *
#**********************************************************************

#**********************************************************************
#* Main                                                               *
#**********************************************************************
# Connection a la base de donnee Oracle
print "Connexion a la base de donnees \n";
my $dbh = DBI->connect( "dbi:Oracle:$ENV{ORACLE_SID}", "$ENV{SUMDB_USER}", "$ENV{SUMDB_PASS}" )
    or die "Connection impossible a la base de donnees";

generateEmail($dbh);

$dbh->disconnect or error ("Probleme de disconnect de la base Oracle");

exit 0;

sub getToEmailAdress {
    my ($dbh) = @_;

    my @listMail;

    my $prep = $dbh->prepare("select cdmEmail from dmTEAM where Audit_Current='Y' and Name = 'EMIR'")
        or die "Error SQl statement";

    $prep->execute() or die "Echec requete\n";

    while (my $email = $prep->fetchrow_array ){
        if($email ne " "){
            push (@listMail, split /,/, $email);
        }
    }

    $prep->finish();

    return @listMail;
}

sub generateEmail {
    my ($dbh) = @_;

    my $message .="<body><html>";
    $message .="<p>Bonjour,</p>";

    my $date = "$ENV{BAT_JM1_DD}-$ENV{BAT_JM1_MM}-$ENV{BAT_JM1_YYYY}";
    #$message .="Recapitulatif Envoi EMIR Positon au " . $date . " : </p>";
         $message = "<body><html>Bonjour,<br /><br />Veuillez trouver ci-joint le rapport Envoi EMIR COLLAT au " . $date . " :<br /><br />";

    $message .= "<p><table style=\"border-collapse: collapse;border:1px solid black;text-align:center;\">";

    $message .= "<tr style=\"height:50px;border:1px solid black\">";
    $message .= "<th style=\"padding: 0px 30px;border:1px solid black\">Company</th>";
    $message .= "<th style=\"padding: 0px 20px;border:1px solid black\">Sent</th>";
    $message .= "<th style=\"padding: 0px 10px;border:1px solid black\">ACPT</th>";
    $message .= "<th style=\"padding: 0px 10px;border:1px solid black\">RJCT</th>";
    $message .= "<th style=\"padding: 0px 10px;border:1px solid black\">No Reply</th>";
         $message .= "<th style=\"padding: 0px 10px;border:1px solid black\">Status</th>";
    $message .= "</tr>";

         $date = "03-05-2024";

        my $sql = "SELECT ";
    $sql .= " cust.id COMPANY, ";
    $sql .= " ES.SENT, NVL(EA.ACPT, 0) ACPT, ";
    $sql .= " NVL(EN.RJCT, 0) RJCT, ";
    $sql .= " NVL((ES.SENT - nvl(EA.ACPT,0) - nvl(EN.RJCT,0)), 0) NOREPLY ";
    $sql .= " FROM ";
    $sql .= " (SELECT RPTGCTRPTY, count(*) AS SENT FROM cdmEmirMarginActivity_HIST ";
    $sql .= " WHERE ";
    $sql .= " TO_CHAR(RPTGTMSTMP, 'DD-MM-YYYY') > '" . $date . "' ";
    $sql .= " AND STATUS ='SENT' GROUP BY RPTGCTRPTY) ES ";
    $sql .= " LEFT JOIN ";
    $sql .= " (SELECT RPTGCTRPTY, count(*) AS ACPT ";
    $sql .= " FROM cdmEmirMarginActivity_HIST ";
    $sql .= " WHERE TO_CHAR(RPTGTMSTMP, 'DD-MM-YYYY') > '" . $date . "' ";
    $sql .= " and STATUS ='ACPT' ";
    $sql .= " GROUP BY RPTGCTRPTY) EA ON ES.RPTGCTRPTY = EA.RPTGCTRPTY ";
    $sql .= " LEFT JOIN ";
    $sql .= " (SELECT RPTGCTRPTY, count(*) AS RJCT ";
    $sql .= " FROM cdmEmirMarginActivity_HIST ";
    $sql .= " WHERE TO_CHAR(RPTGTMSTMP, 'DD-MM-YYYY') > '" . $date . "' ";
    $sql .= " AND STATUS ='RJCT' ";
    $sql .= " GROUP BY RPTGCTRPTY) EN ON ES.RPTGCTRPTY = EN.RPTGCTRPTY ";
    $sql .= " LEFT JOIN ";
    $sql .= " (SELECT id, cdmlei FROM dmCUSTOMER ";
    $sql .= " WHERE audit_current='Y' ";
    $sql .= " AND audit_entitystate='VER') ";
    $sql .= " cust on cust.cdmlei = ES.RPTGCTRPTY ";
    $sql .= " ORDER BY cust.id ";

         printf("$sql");

    my $prep = $dbh->prepare($sql)
        or die "Error SQl statement";

    my $rv = $prep->execute() or die "Echec requete\n";

    my $sentContracts = 0;
    my $ackContracts = 0;
    my $nackContracts = 0;
    my $noReplyContracts = 0;

    while (my @row = $prep->fetchrow_array() )
    {
        #print "@row\n";
        $sentContracts += $row[1];
        $ackContracts += $row[2];
        $nackContracts += $row[3];
        $noReplyContracts += $row[4];

        if ( $row[1] != ($row[2] + $row[3]) )
        {
            $message .= "<tr style=\"color:red;height:50px;border:1px solid black\">";
            $message .= "<th style=\"padding: 0px 30px;border:1px solid black\">" . $row[0] . "</th>";
            $message .= "<th style=\"padding: 0px 20px;border:1px solid black\">" . $row[1] . "</th>";
            $message .= "<th style=\"padding: 0px 10px;border:1px solid black\">" . $row[2] . "</th>";
            $message .= "<th style=\"padding: 0px 10px;border:1px solid black\">" . $row[3] . "</th>";
            $message .= "<th style=\"padding: 0px 10px;border:1px solid black\">" . $row[4] . "</th>";
            $message .= "<th style=\"padding: 0px 10px;border:1px solid black\">NOK</th>";
            $message .= "</tr>";
        }
        else
        {
            $message .= "<tr style=\"color:green;height:50px;border:1px solid black\">";
            $message .= "<th style=\"padding: 0px 30px;border:1px solid black\">" . $row[0] . "</th>";
            $message .= "<th style=\"padding: 0px 20px;border:1px solid black\">" . $row[1] . "</th>";
            $message .= "<th style=\"padding: 0px 10px;border:1px solid black\">" . $row[2] . "</th>";
            $message .= "<th style=\"padding: 0px 10px;border:1px solid black\">" . $row[3] . "</th>";
            $message .= "<th style=\"padding: 0px 10px;border:1px solid black\">" . $row[4] . "</th>";
            $message .= "<th style=\"padding: 0px 10px;border:1px solid black\">OK</th>";
            $message .= "</tr>";
        }
    }

    if ( $sentContracts != ($ackContracts + $nackContracts) )
    {
        $message .= "<tr style=\"color:red;height:50px;border:1px solid black\">";
        $message .= "<th style=\"padding: 0px 30px;border:1px solid black\">Total</th>";
        $message .= "<th style=\"padding: 0px 20px;border:1px solid black\">" . $sentContracts . "</th>";
        $message .= "<th style=\"padding: 0px 10px;border:1px solid black\">" . $ackContracts . "</th>";
        $message .= "<th style=\"padding: 0px 10px;border:1px solid black\">" . $nackContracts . "</th>";
        $message .= "<th style=\"padding: 0px 10px;border:1px solid black\">" . $noReplyContracts . "</th>";
        $message .= "<th style=\"padding: 0px 10px;border:1px solid black\">NOK</th>";
        $message .= "</tr>";
    }
    else
    {
        $message .= "<tr style=\"color:black;height:50px;border:1px solid black\">";
        $message .= "<th style=\"padding: 0px 30px;border:1px solid black\">Total</th>";
        $message .= "<th style=\"padding: 0px 20px;border:1px solid black\">" . $sentContracts . "</th>";
        $message .= "<th style=\"padding: 0px 10px;border:1px solid black\">" . $ackContracts . "</th>";
        $message .= "<th style=\"padding: 0px 10px;border:1px solid black\">" . $nackContracts . "</th>";
        $message .= "<th style=\"padding: 0px 10px;border:1px solid black\">" . $noReplyContracts . "</th>";
        $message .= "<th style=\"padding: 0px 10px;border:1px solid black\">OK</th>";
        $message .= "</tr>";
    }

    $prep->finish();

    $message .="</table></p>";
    $message .="<p>Cordialement</p>\n";
    $message .="</body></html>";

    sendEmail($dbh, $message);
}

sub sendEmail {
    my ($dbh, $message) = @_;
    my $environment = "$ENV{CNCEENV}";
    my @mails = getToEmailAdress($dbh);

    # Pour test
    my @mails = ();
    my $user_to = "MOE_INFRA_PTF\@bpce.fr;";
    my $user_to = "abderrazak.derdouri_ext\@bpce.fr;";
    push (@mails, $user_to);

    my $smtpServeur = $ENV{SMTP_SERVEUR};
    my $smtp = Net::SMTP->new($smtpServeur, Timeout => 5, Debug => 0)
        or die "Impossible de se connecter au serveur SMTP \n";

    my $user_from = "MOE_INFRA_PTF\@bpce.fr";

    my $date = "$ENV{BAT_JM1_DD}-$ENV{BAT_JM1_MM}-$ENV{BAT_JM1_YYYY}";
    my $subjet = "[$ENV{CNCEENV}]" . " Declaration EMIR COLLAT en date du " . $date;

    if ($smtp->mail($user_from) && $smtp->to(@mails))
    {
        $smtp->data();
        $smtp->datasend("From: " . $user_from. "\n");
        $smtp->datasend("To: " . @mails . "\n");
        $smtp->datasend("Subject: " . $subjet . "\n");

        $smtp->datasend("MIME-Version: 1.0\n");
        $smtp->datasend("Content-Type: multipart/mixed; boundary=\"separator\"\n\n");
        $smtp->datasend("This is multipart MIME message\n");

        $smtp->datasend("--separator\n");
        $smtp->datasend("Content-Type: text/html; charset=\"iso-8859-1\"\n");
        $smtp->datasend("Content-Transfer-Encoding: 8bit\n\n");

        $smtp->datasend($message);

        $smtp->datasend("\n");
        $smtp->dataend;
    }
    else
    {
        printf $smtp->message;
    }

    $smtp->quit;
}
